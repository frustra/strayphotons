#include "LaserSensor.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    template<>
    bool Component<LaserSensor>::Load(const EntityScope &scope, LaserSensor &dst, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "threshold") {
                if (!sp::json::Load(dst.threshold, param.second)) {
                    Errorf("Invalid laser_sensor threshold: %s", param.second.to_str());
                    return false;
                }
            }
        }
        return true;
    }
} // namespace ecs
