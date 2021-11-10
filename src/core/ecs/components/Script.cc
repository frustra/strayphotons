#include "Script.hh"

#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Script>::Load(sp::Scene *scene, Script &dst, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "onTick") {
                if (param.second.is<picojson::array>()) {
                    for (auto nameParam : param.second.get<picojson::array>()) {
                        auto scriptName = nameParam.get<std::string>();
                        auto it = ScriptDefinitions.find(scriptName);
                        if (it != ScriptDefinitions.end()) {
                            dst.AddOnTick(it->second);
                        } else {
                            Errorf("Script has unknown onTick event: %s", scriptName);
                            return false;
                        }
                    }
                } else {
                    auto scriptName = param.second.get<std::string>();
                    auto it = ScriptDefinitions.find(scriptName);
                    if (it != ScriptDefinitions.end()) {
                        dst.AddOnTick(it->second);
                    } else {
                        Errorf("Script has unknown onTick event: %s", scriptName);
                        return false;
                    }
                }
            } else if (param.first == "parameters") {
                for (auto scriptParam : param.second.get<picojson::object>()) {
                    if (scriptParam.second.is<std::string>()) {
                        dst.SetParam(scriptParam.first, scriptParam.second.get<std::string>());
                    } else if (scriptParam.second.is<bool>()) {
                        dst.SetParam(scriptParam.first, scriptParam.second.get<bool>());
                    } else {
                        dst.SetParam(scriptParam.first, scriptParam.second.get<double>());
                    }
                }
            }
        }
        return true;
    }

    void Script::CopyCallbacks(const Script &src) {
        if (onTickCallbacks.empty()) {
            onTickCallbacks = src.onTickCallbacks;
        } else {
            Errorf("Script::CopyCallbacks called on non-empty script");
        }
    }

    void Script::CopyParams(const Script &src) {
        for (auto &[name, param] : src.scriptParameters) {
            scriptParameters.insert_or_assign(name, param);
        }
    }
} // namespace ecs
