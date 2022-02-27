#include "Light.hh"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<Light>::Load(ScenePtr scenePtr, Light &light, const picojson::value &src) {
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
                light.gelName = param.second.get<string>();
            } else if (param.first == "on") {
                light.on = param.second.get<bool>();
            } else if (param.first == "shadowMapSize") {
                light.shadowMapSize = (uint32)param.second.get<double>();
            } else if (param.first == "shadowMapClip") {
                light.shadowMapClip = sp::MakeVec2(param.second);
            }
        }
        return true;
    }
} // namespace ecs
