#include "LaserLine.hh"

#include "assets/AssetHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

#include <picojson/picojson.h>

namespace ecs {
    template<>
    bool Component<LaserLine>::Load(const EntityScope &scope, LaserLine &dst, const picojson::value &src) {
        ecs::LaserLine::Line line;
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "intensity") {
                dst.intensity = param.second.get<double>();
            } else if (param.first == "radius") {
                dst.radius = param.second.get<double>();
            } else if (param.first == "color") {
                line.color = sp::MakeVec3(param.second);
            } else if (param.first == "on") {
                dst.on = param.second.get<bool>();
            } else if (param.first == "points") {
                for (auto point : param.second.get<picojson::array>()) {
                    line.points.push_back(sp::MakeVec3(point));
                }
            }
        }
        dst.line = std::move(line);
        return true;
    }
} // namespace ecs
