#include "Script.hh"

#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <atomic>
#include <picojson/picojson.h>

namespace ecs {
    ScriptDefinitions &GetScriptDefinitions() {
        static ScriptDefinitions scriptDefinitions;
        return scriptDefinitions;
    }

    static std::atomic_size_t nextInstanceId;

    ScriptState::ScriptState(const EntityScope &scope)
        : scope(scope), callback(std::monostate()), instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const EntityScope &scope, OnTickFunc callback)
        : scope(scope), callback(callback), instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const EntityScope &scope, OnPhysicsUpdateFunc callback)
        : scope(scope), callback(callback), instanceId(++nextInstanceId) {}
    ScriptState::ScriptState(const EntityScope &scope, PrefabFunc callback)
        : scope(scope), callback(callback), instanceId(++nextInstanceId) {}

    bool parseScriptState(ScriptState &state, const picojson::value &src) {
        bool isOnEvent = false;
        const auto &definitions = GetScriptDefinitions();
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
                        state.callback = it->second;
                    } else {
                        Errorf("Script has unknown onTick definition: %s", scriptName);
                        return false;
                    }
                } else {
                    Errorf("Script onTick has invalid definition: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "onEvent") {
                if (param.second.is<std::string>()) {
                    auto scriptName = param.second.get<std::string>();
                    auto it = definitions.scripts.find(scriptName);
                    if (it != definitions.scripts.end()) {
                        if (state) {
                            Errorf("Script has multiple definitions: %s", scriptName);
                            return false;
                        }
                        state.callback = it->second;
                        isOnEvent = true;
                    } else {
                        Errorf("Script has unknown onEvent definition: %s", scriptName);
                        return false;
                    }
                } else {
                    Errorf("Script onEvent has invalid definition: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "onPhysicsUpdate") {
                if (param.second.is<std::string>()) {
                    auto scriptName = param.second.get<std::string>();
                    auto it = definitions.physicsUpdates.find(scriptName);
                    if (it != definitions.physicsUpdates.end()) {
                        if (state) {
                            Errorf("Script has multiple definitions: %s", scriptName);
                            return false;
                        }
                        state.callback = it->second;
                    } else {
                        Errorf("Script has unknown onPhysicsUpdate definition: %s", scriptName);
                        return false;
                    }
                } else {
                    Errorf("Script onPhysicsUpdate has invalid definition: %s", param.second.to_str());
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
                        state.callback = it->second;
                    } else {
                        Errorf("Script has unknown prefab definition: %s", scriptName);
                        return false;
                    }
                } else {
                    Errorf("Script prefab has invalid definition: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "filter_events") {
                if (param.second.is<std::string>()) {
                    state.filterEvents.emplace_back(param.second.get<std::string>());
                } else if (param.second.is<picojson::array>()) {
                    for (auto subParam : param.second.get<picojson::array>()) {
                        state.filterEvents.emplace_back(subParam.get<std::string>());
                    }
                }
            } else if (param.first == "parameters") {
                for (auto scriptParam : param.second.get<picojson::object>()) {
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
        if (isOnEvent && state.filterEvents.empty()) {
            Errorf("onEvent script has no event filters");
            return false;
        }
        if (!isOnEvent && !state.filterEvents.empty()) {
            Errorf("Only onEvent scripts can have event filters");
            return false;
        }
        return true;
    }

    template<>
    bool Component<Script>::Load(const EntityScope &scope, Script &dst, const picojson::value &src) {
        if (src.is<picojson::object>()) {
            ScriptState state(scope);
            if (!parseScriptState(state, src)) return false;
            dst.scripts.push_back(std::move(state));
        } else if (src.is<picojson::array>()) {
            for (auto entry : src.get<picojson::array>()) {
                if (entry.is<picojson::object>()) {
                    ScriptState state(scope);
                    if (!parseScriptState(state, entry)) return false;
                    dst.scripts.push_back(std::move(state));
                } else {
                    Errorf("Invalid script entry: %s", entry.to_str());
                    return false;
                }
            }
        } else {
            Errorf("Invalid script component: %s", src.to_str());
            return false;
        }
        return true;
    }

    template<>
    void Component<Script>::Apply(const Script &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstScript = dst.Get<Script>(lock);
        for (auto &script : src.scripts) {
            if (!sp::contains(dstScript.scripts, script)) dstScript.scripts.emplace_back(script);
        }
    }

    void Script::OnTick(Lock<WriteAll> lock, const Entity &ent, chrono_clock::duration interval) {
        for (auto &state : scripts) {
            auto callback = std::get_if<OnTickFunc>(&state.callback);
            if (callback) {
                if (!state.filterEvents.empty()) {
                    Assertf(ent.Has<EventInput>(lock),
                        "OnEvent script is missing EventInput: %s",
                        ecs::ToString(lock, ent));
                    auto &eventInput = ent.Get<EventInput>(lock);
                    bool hasEvents = false;
                    for (auto &queueName : state.filterEvents) {
                        if (eventInput.HasEvents(queueName)) {
                            hasEvents = true;
                            break;
                        }
                    }
                    if (!hasEvents) continue;
                }
                ZoneScopedN("OnTick");
                ZoneStr(ecs::ToString(lock, ent));
                (*callback)(state, lock, ent, interval);
            }
        }
    }

    void Script::OnPhysicsUpdate(PhysicsUpdateLock lock, const Entity &ent, chrono_clock::duration interval) {
        ZoneScopedN("OnPhysicsUpdate");
        // ZoneStr(ecs::ToString(lock, ent));
        for (auto &state : scripts) {
            auto callback = std::get_if<OnPhysicsUpdateFunc>(&state.callback);
            if (callback) (*callback)(state, lock, ent, interval);
        }
    }

    void Script::Prefab(Lock<AddRemove> lock, const Entity &ent) {
        ZoneScopedN("Prefab");
        ZoneStr(ecs::ToString(lock, ent));
        for (auto &state : scripts) {
            auto callback = std::get_if<PrefabFunc>(&state.callback);
            if (callback) (*callback)(state, lock, ent);
        }
    }
} // namespace ecs
