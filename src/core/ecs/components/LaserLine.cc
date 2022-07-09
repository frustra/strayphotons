#include "LaserLine.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    template<>
    bool Component<LaserLine>::Load(const EntityScope &scope, LaserLine &dst, const picojson::value &src) {
        ecs::LaserLine::Line line;
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "intensity") {
                dst.intensity = param.second.get<double>();
            } else if (param.first == "radius") {
                dst.radius = param.second.get<double>();
            } else if (param.first == "media_density") {
                dst.mediaDensityFactor = param.second.get<double>();
            } else if (param.first == "color") {
                if (!sp::json::Load(line.color, param.second)) {
                    Errorf("Invalid line color: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "on") {
                dst.on = param.second.get<bool>();
            } else if (param.first == "points") {
                for (auto &p : param.second.get<picojson::array>()) {
                    auto &point = line.points.emplace_back();
                    if (!sp::json::Load(point, p)) {
                        Errorf("Invalid line point: %s", p.to_str());
                        return false;
                    }
                }
            }
        }
        dst.line = std::move(line);
        return true;
    }

    template<>
    bool Component<LaserLine>::Save(Lock<Read<Name>> lock, picojson::value &dst, const LaserLine &src) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        static const LaserLine::Line defaultLine = {};
        static const LaserLine::Segment defaultSegment = {};

        std::visit(
            [&](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, LaserLine::Line>) {
                    sp::json::SaveIfChanged(obj, "color", arg.color, defaultLine.color);

                    if (arg.points.empty()) return;
                    picojson::array points;
                    points.reserve(arg.points.size());
                    for (auto &point : arg.points) {
                        sp::json::Save(points.emplace_back(), point);
                    }
                    obj["points"] = picojson::value(points);
                } else if constexpr (std::is_same_v<T, LaserLine::Segments>) {
                    if (arg.empty()) return;
                    picojson::array segments;
                    segments.reserve(arg.size());
                    for (auto &segment : arg) {
                        picojson::object segmentObj;
                        sp::json::SaveIfChanged(segmentObj, "color", segment.color, defaultSegment.color);
                        sp::json::Save(segmentObj["start"], segment.start);
                        sp::json::Save(segmentObj["end"], segment.end);
                        segments.emplace_back(segmentObj);
                    }
                    obj["segments"] = picojson::value(segments);
                } else {
                    Abortf("Unsupported laser line type: %s", typeid(arg).name());
                }
            },
            src.line);
        return true;
    }
} // namespace ecs
