#include "LaserEmitter.hh"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<LaserEmitter>::Load(const EntityScope &scope, LaserEmitter &dst, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "intensity") {
                dst.intensity = param.second.get<double>();
            } else if (param.first == "color") {
                dst.color = sp::MakeVec3(param.second);
            } else if (param.first == "on") {
                dst.on = param.second.get<bool>();
            }
        }
        return true;
    }
} // namespace ecs
