#include "Light.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Light>::Load(Lock<Read<ecs::Name>> lock, Light &light, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "intensity") {
                light.intensity = param.second.get<double>();
            } else if (param.first == "illuminance") {
                light.illuminance = param.second.get<double>();
            } else if (param.first == "spotAngle") {
                light.spotAngle = glm::radians(param.second.get<double>());
            } else if (param.first == "tint") {
                light.tint = sp::MakeVec3(param.second);
            } else if (param.first == "gel") {
                light.gelId = param.second.get<bool>() ? 1 : 0;
            } else if (param.first == "on") {
                light.on = param.second.get<bool>();
            } else if (param.first == "bulb") {
                auto bulbName = param.second.get<string>();
                for (auto ent : lock.EntitiesWith<Name>()) {
                    if (ent.Get<Name>(lock) == bulbName) {
                        light.bulb = ent;
                        break;
                    }
                }
            }
        }
        return true;
    }
} // namespace ecs
