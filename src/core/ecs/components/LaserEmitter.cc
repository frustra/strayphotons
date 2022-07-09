#include "LaserEmitter.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    template<>
    bool Component<LaserEmitter>::Load(const EntityScope &scope, LaserEmitter &dst, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "intensity") {
                dst.intensity = param.second.get<double>();
            } else if (param.first == "color") {
                if (!sp::json::Load(dst.color, param.second)) {
                    Errorf("Invalid laser_emitter color: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "on") {
                dst.on = param.second.get<bool>();
            }
        }
        return true;
    }
} // namespace ecs
