#include "Scripts.hh"

#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <atomic>

namespace ecs {
    ScriptDefinitions &GetScriptDefinitions() {
        static ScriptDefinitions scriptDefinitions;
        return scriptDefinitions;
    }

    void ScriptDefinitions::RegisterScript(ScriptDefinition &&definition) {
        Assertf(!scripts.contains(definition.name), "Script definition already exists: %s", definition.name);
        scripts.emplace(definition.name, definition);
    }

    void ScriptDefinitions::RegisterPrefab(ScriptDefinition &&definition) {
        Assertf(!prefabs.contains(definition.name), "Prefab definition already exists: %s", definition.name);
        prefabs.emplace(definition.name, definition);
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
        auto &srcObj = src.get<picojson::object>();
        for (auto param : srcObj) {
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
            }
        }
        if (std::holds_alternative<std::monostate>(state.definition.callback)) {
            Errorf("Script has no definition: %s", src.to_str());
            return false;
        }
        if (state.definition.context) {
            // Access will initialize default parameters
            void *dataPtr = state.definition.context->Access(state);
            Assertf(dataPtr, "Script definition returned null data: %s", state.definition.name);

            auto it = srcObj.find("parameters");
            if (it != srcObj.end()) {
                for (auto &field : state.definition.context->metadata.fields) {
                    if (!field.Load(scope, dataPtr, it->second)) {
                        Errorf("Script %s has invalid parameter: %s", state.definition.name, field.name);
                        return false;
                    }
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

            if (src.definition.context) {
                const void *dataPtr = src.definition.context->Access(src);
                const void *defaultPtr = src.definition.context->GetDefault();
                Assertf(dataPtr, "Script definition returned null data: %s", src.definition.name);
                for (auto &field : src.definition.context->metadata.fields) {
                    field.Save(scope, obj["parameters"], dataPtr, defaultPtr);
                }
            }
        }
    }

    template<>
    void Component<Scripts>::Apply(Scripts &dst, const Scripts &src, bool liveTarget) {
        for (auto &script : src.scripts) {
            if (!sp::contains(dst.scripts, script)) dst.scripts.emplace_back(script);
        }
    }

    void Scripts::OnTick(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval) {
        for (auto &state : scripts) {
            auto callback = std::get_if<OnTickFunc>(&state.definition.callback);
            if (callback && *callback) {
                if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
                ZoneScopedN("OnTick");
                ZoneStr(ecs::ToString(lock, ent));
                (*callback)(state, lock, ent, interval);
            }
        }
    }

    void Scripts::OnPhysicsUpdate(PhysicsUpdateLock lock, const Entity &ent, chrono_clock::duration interval) {
        for (auto &state : scripts) {
            auto callback = std::get_if<OnPhysicsUpdateFunc>(&state.definition.callback);
            if (callback && *callback) {
                if (state.definition.filterOnEvent && state.eventQueue && state.eventQueue->Empty()) continue;
                ZoneScopedN("OnPhysicsUpdate");
                ZoneStr(ecs::ToString(lock, ent));
                (*callback)(state, lock, ent, interval);
            }
        }
    }

    void Scripts::RunPrefabs(Lock<AddRemove> lock, Entity ent) {
        if (!ent.Has<Scripts, SceneInfo>(lock)) return;

        ZoneScopedN("RunPrefabs");
        ZoneStr(ecs::ToString(lock, ent));

        auto scene = ent.Get<const SceneInfo>(lock).scene.lock();
        Assertf(scene, "RunPrefabs entity has null scene: %s", ecs::ToString(lock, ent));

        // Prefab scripts may add additional scripts while iterating.
        // Script state references may not be valid if storage is resized,
        // so we need to reference the lock every loop iteration.
        for (size_t i = 0; i < ent.Get<const Scripts>(lock).scripts.size(); i++) {
            // Create a read-only copy of the script state so the passed reference is stable.
            auto state = ent.Get<const Scripts>(lock).scripts[i];
            auto callback = std::get_if<PrefabFunc>(&state.definition.callback);
            if (callback && *callback) (*callback)(state, scene, lock, ent);
        }
    }

    const ScriptState *Scripts::FindScript(size_t instanceId) const {
        auto it = std::find_if(scripts.begin(), scripts.end(), [&](auto &arg) {
            return arg.GetInstanceId() == instanceId;
        });
        return it != scripts.end() ? &*it : nullptr;
    }
} // namespace ecs
