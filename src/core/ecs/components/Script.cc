#include "Script.hh"

#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Script>::Load(sp::Scene *scene, Script &dst, const picojson::value &src) {
        for (auto entry : src.get<picojson::array>()) {
            if (entry.is<picojson::object>()) {
                ScriptState state;
                for (auto param : entry.get<picojson::object>()) {
                    if (param.first == "onTick") {
                        auto scriptName = param.second.get<std::string>();
                        auto it = ScriptDefinitions.find(scriptName);
                        if (it != ScriptDefinitions.end()) {
                            state = ScriptState(it->second);
                        } else {
                            Errorf("Script has unknown onTick event: %s", scriptName);
                            return false;
                        }
                    } else if (param.first == "prefab") {
                        auto scriptName = param.second.get<std::string>();
                        auto it = PrefabDefinitions.find(scriptName);
                        if (it != PrefabDefinitions.end()) {
                            state = ScriptState(it->second);
                        } else {
                            Errorf("Script has unknown prefab definition: %s", scriptName);
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
                if (state) {
                    dst.scripts.push_back(std::move(state));
                }
            }
        }
        return true;
    }

    template<>
    void Component<Script>::Apply(const Script &src, Lock<AddRemove> lock, Entity dst) {
        auto &dstScript = dst.Get<Script>(lock);
        if (dstScript.scripts.empty()) dstScript.scripts.assign(src.scripts.begin(), src.scripts.end());
    }
} // namespace ecs
