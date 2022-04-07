#include "LaserSensor.hh"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<LaserSensor>::Load(ScenePtr scenePtr,
        const Name &scope,
        LaserSensor &dst,
        const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "threshold") { dst.threshold = sp::MakeVec3(param.second); }
        }
        return true;
    }
} // namespace ecs
