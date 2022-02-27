#include "Script.hh"

#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    bool parseScriptState(ScriptState &state, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "onTick") {
                if (param.second.is<std::string>()) {
                    auto scriptName = param.second.get<std::string>();
                    auto it = ScriptDefinitions.find(scriptName);
                    if (it != ScriptDefinitions.end()) {
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
            } else if (param.first == "prefab") {
                if (param.second.is<std::string>()) {
                    auto scriptName = param.second.get<std::string>();
                    auto it = PrefabDefinitions.find(scriptName);
                    if (it != PrefabDefinitions.end()) {
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
        return state;
    }

    template<>
    bool Component<Script>::Load(ScenePtr scenePtr, Script &dst, const picojson::value &src) {
        if (src.is<picojson::object>()) {
            ScriptState state(scenePtr);
            if (parseScriptState(state, src)) dst.scripts.push_back(std::move(state));
        } else if (src.is<picojson::array>()) {
            for (auto entry : src.get<picojson::array>()) {
                if (entry.is<picojson::object>()) {
                    ScriptState state(scenePtr);
                    if (parseScriptState(state, entry)) dst.scripts.push_back(std::move(state));
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
        if (dstScript.scripts.empty()) dstScript.scripts.assign(src.scripts.begin(), src.scripts.end());
    }
} // namespace ecs
