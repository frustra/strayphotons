#include "LightSensor.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"

namespace ecs {
    template<>
    bool Component<LightSensor>::Load(const EntityScope &scope, LightSensor &sensor, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "translate") {
                if (!sp::json::Load(sensor.position, param.second)) {
                    Errorf("Invalid light_sensor position: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "direction") {
                if (!sp::json::Load(sensor.direction, param.second)) {
                    Errorf("Invalid light_sensor direction: %s", param.second.to_str());
                    return false;
                }
            }
        }
        return true;
    }
} // namespace ecs
