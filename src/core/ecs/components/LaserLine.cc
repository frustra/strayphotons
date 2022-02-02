#include "LaserLine.hh"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<LaserLine>::Load(sp::Scene *scene, LaserLine &dst, const picojson::value &src) {
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "intensity") {
                dst.intensity = param.second.get<double>();
            } else if (param.first == "color") {
                dst.color = sp::MakeVec4(param.second);
            } else if (param.first == "on") {
                dst.on = param.second.get<bool>();
            } else if (param.first == "points") {
                for (auto point : param.second.get<picojson::array>()) {
                    dst.points.push_back(sp::MakeVec3(point));
                }
            }
        }
        return true;
    }
} // namespace ecs
