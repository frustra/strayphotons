#include "LightSensor.hh"

#include <assets/AssetHelpers.hh>
#include <ecs/EcsImpl.hh>
#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<LightSensor>::Load(ScenePtr scenePtr, LightSensor &sensor, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "translate") {
                sensor.position = sp::MakeVec3(param.second);
            } else if (param.first == "direction") {
                sensor.direction = sp::MakeVec3(param.second);
            }
        }
        return true;
    }
} // namespace ecs
