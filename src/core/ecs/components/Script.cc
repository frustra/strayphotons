#include "Script.hh"

#include <core/Logging.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Script>::Load(Lock<Read<ecs::Name>> lock, Script &dst, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "onTick") {
                auto scriptName = param.second.get<std::string>();
                auto it = ScriptDefinitions.find(scriptName);
                if (it != ScriptDefinitions.end()) {
                    dst.AddOnTick(it->second);
                } else {
                    Errorf("Script has unknown onTick event: %s", scriptName);
                    return false;
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
} // namespace ecs
