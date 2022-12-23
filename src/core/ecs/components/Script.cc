#include "Script.hh"

#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <atomic>

namespace ecs {
    ScriptDefinitions &GetScriptDefinitions() {
        static ScriptDefinitions scriptDefinitions;
        return scriptDefinitions;
    }

    static std::atomic_size_t nextInstanceId;

    ScriptState::ScriptState() : instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const EntityScope &scope, const ScriptDefinition &definition)
        : scope(scope), definition(definition), instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const EntityScope &scope, OnTickFunc callback)
        : scope(scope), definition({"", {}, false, nullptr, callback}), instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const EntityScope &scope, OnPhysicsUpdateFunc callback)
        : scope(scope), definition({"", {}, false, nullptr, callback}), instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const EntityScope &scope, PrefabFunc callback)
        : scope(scope), definition({"", {}, false, nullptr, callback}), instanceId(++nextInstanceId) {}

    template<>
    bool StructMetadata::Load<ScriptState>(const EntityScope &scope, ScriptState &state, const picojson::value &src) {
        const auto &definitions = GetScriptDefinitions();
        state.scope = scope;
        picojson::value parameters;
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "onTick") {
                if (param.second.is<std::string>()) {
                    auto scriptName = param.second.get<std::string>();
                    auto it = definitions.scripts.find(scriptName);
                    if (it != definitions.scripts.end()) {
                        if (state) {
                            Errorf("Script has multiple definitions: %s", scriptName);
                            return false;
                        }
                        state.definition = it->second;
                    } else {
                        Errorf("Script has unknown onTick definition: %s", scriptName);
                        return false;
                    }
                } else {
                    Errorf("Script onTick has invalid definition: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "prefab") {
                if (param.second.is<std::string>()) {
                    auto scriptName = param.second.get<std::string>();
                    auto it = definitions.prefabs.find(scriptName);
                    if (it != definitions.prefabs.end()) {
                        if (state) {
                            Errorf("Script has multiple definitions: %s", scriptName);
                            return false;
                        }
                        state.definition = it->second;
                    } else {
                        Errorf("Script has unknown prefab definition: %s", scriptName);
                        return false;
                    }
                } else {
                    Errorf("Script prefab has invalid definition: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "parameters") {
                parameters = param.second;
                for (auto scriptParam : parameters.get<picojson::object>()) {
                    if (scriptParam.second.is<picojson::array>()) {
                        auto array = scriptParam.second.get<picojson::array>();
                        if (array.empty()) continue;
                        if (array.front().is<std::string>()) {
                            std::vector<std::string> list;
                            for (auto arrayParam : array) {
                                list.emplace_back(arrayParam.get<std::string>());
                            }
                            state.SetParam(scriptParam.first, list);
                        } else if (array.front().is<bool>()) {
                            std::vector<bool> list;
                            for (auto arrayParam : array) {
                                list.emplace_back(arrayParam.get<bool>());
                            }
                            state.SetParam(scriptParam.first, list);
                        } else if (array.front().is<double>()) {
                            std::vector<double> list;
                            for (auto arrayParam : array) {
                                list.emplace_back(arrayParam.get<double>());
                            }
                            state.SetParam(scriptParam.first, list);
                        }
                    } else if (scriptParam.second.is<std::string>()) {
                        state.SetParam(scriptParam.first, scriptParam.second.get<std::string>());
                    } else if (scriptParam.second.is<bool>()) {
                        state.SetParam(scriptParam.first, scriptParam.second.get<bool>());
                    } else {
                        state.SetParam(scriptParam.first, scriptParam.second.get<double>());
                    }
                }
            }
        }
        if (std::holds_alternative<std::monostate>(state.definition.callback)) {
            Errorf("Script has no definition: %s", src.to_str());
            return false;
        }
        if (state.definition.context) {
            void *dataPtr = state.definition.context->Access(state);
            if (!dataPtr) {
                Errorf("Script definition returned null data: %s", state.definition.name);
                return false;
            }
            for (auto &field : state.definition.context->metadata.fields) {
                if (!field.Load(scope, dataPtr, parameters)) {
                    Errorf("Script %s has invalid parameter: %s", state.definition.name, field.name);
                    return false;
                }
            }
        }
        return true;
    }

    template<>
    void StructMetadata::Save<ScriptState>(const EntityScope &scope, picojson::value &dst, const ScriptState &src) {
        if (src.definition.name.empty()) {
            dst = picojson::value("inline C++ lambda");
        } else if (!std::holds_alternative<std::monostate>(src.definition.callback)) {
            if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
            auto &obj = dst.get<picojson::object>();
            if (std::holds_alternative<PrefabFunc>(src.definition.callback)) {
                obj["prefab"] = picojson::value(src.definition.name);
            } else {
                obj["onTick"] = picojson::value(src.definition.name);
            }
            if (!src.parameters.empty()) {
                auto &paramValue = obj["parameters"];
                paramValue.set<picojson::object>({});
                auto &paramObj = paramValue.get<picojson::object>();
                for (auto &[name, param] : src.parameters) {
                    std::visit(
                        [&](auto &&arg) {
                            sp::json::Save(scope, paramObj[name], arg);
                        },
                        param);
                }
            }
        }
    }

    template<>
    void Component<Script>::Apply(Script &dst, const Script &src, bool liveTarget) {
        for (auto &script : src.scripts) {
            if (!sp::contains(dst.scripts, script)) dst.scripts.emplace_back(script);
        }
    }

    void Script::OnTick(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval) {
        for (auto &state : scripts) {
            auto callback = std::get_if<OnTickFunc>(&state.definition.callback);
            if (callback) {
                if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
                ZoneScopedN("OnTick");
                ZoneStr(ecs::ToString(lock, ent));
                (*callback)(state, lock, ent, interval);
            }
        }
    }

    void Script::OnPhysicsUpdate(PhysicsUpdateLock lock, const Entity &ent, chrono_clock::duration interval) {
        for (auto &state : scripts) {
            auto callback = std::get_if<OnPhysicsUpdateFunc>(&state.definition.callback);
            if (callback) {
                if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
                ZoneScopedN("OnPhysicsUpdate");
                ZoneStr(ecs::ToString(lock, ent));
                (*callback)(state, lock, ent, interval);
            }
        }
    }

    void Script::Prefab(Lock<AddRemove> lock, const Entity &ent) {
        ZoneScopedN("Prefab");
        ZoneStr(ecs::ToString(lock, ent));
        // Prefab scripts may add additional scripts while iterating.
        // Script state references may not be valid if storage is resized,
        // so we need to reference the lock every loop iteration.
        for (size_t i = 0; i < ent.Get<const Script>(lock).scripts.size(); i++) {
            // Create a read-only copy of the script state so the passed reference is stable.
            auto state = ent.Get<const Script>(lock).scripts[i];
            auto callback = std::get_if<PrefabFunc>(&state.definition.callback);
            if (callback) (*callback)(state, lock, ent);
        }
    }

    const ScriptState *Script::FindScript(size_t instanceId) const {
        auto it = std::find_if(scripts.begin(), scripts.end(), [&](auto &arg) {
            return arg.GetInstanceId() == instanceId;
        });
        return it != scripts.end() ? &*it : nullptr;
    }
} // namespace ecs
