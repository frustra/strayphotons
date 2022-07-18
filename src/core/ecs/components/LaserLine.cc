#include "LaserLine.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    template<>
    bool Component<LaserLine>::Load(const EntityScope &scope, LaserLine &dst, const picojson::value &src) {
        if (!src.is<picojson::object>()) {
            Errorf("Invalid laser line: %s", src.to_str());
            return false;
        }

        ecs::LaserLine::Line line;
        for (auto param : src.get<picojson::object>()) {
            if (param.first == "color") {
                if (!sp::json::Load(scope, line.color, param.second)) {
                    Errorf("Invalid line color: %s", param.second.to_str());
                    return false;
                }
            } else if (param.first == "points") {
                if (!sp::json::Load(scope, line.points, param.second)) {
                    Errorf("Invalid line points: %s", param.second.to_str());
                    return false;
                }
            }
        }
        dst.line = std::move(line);
        return true;
    }

    template<>
    bool Component<LaserLine>::Save(Lock<Read<Name>> lock,
        const EntityScope &scope,
        picojson::value &dst,
        const LaserLine &src) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &obj = dst.get<picojson::object>();

        static const LaserLine::Line defaultLine = {};
        static const LaserLine::Segment defaultSegment = {};

        if (std::holds_alternative<LaserLine::Line>(src.line)) {
            auto &line = std::get<LaserLine::Line>(src.line);
            sp::json::SaveIfChanged(scope, obj, "color", line.color, defaultLine.color);

            if (line.points.empty()) return true;
            sp::json::Save(scope, obj["points"], line.points);
            return true;
        } else if (std::holds_alternative<LaserLine::Segments>(src.line)) {
            auto &seg = std::get<LaserLine::Segments>(src.line);
            if (seg.empty()) return true;
            picojson::array segments;
            segments.reserve(seg.size());
            for (auto &segment : seg) {
                picojson::object segmentObj;
                sp::json::SaveIfChanged(scope, segmentObj, "color", segment.color, defaultSegment.color);
                sp::json::Save(scope, segmentObj["start"], segment.start);
                sp::json::Save(scope, segmentObj["end"], segment.end);
                segments.emplace_back(segmentObj);
            }
            obj["segments"] = picojson::value(segments);
            return true;
        } else {
            Errorf("Unsupported laser line variant type: %u", src.line.index());
            return false;
        }
    }

    template<>
    void Component<LaserLine>::Apply(const LaserLine &src, Lock<AddRemove> lock, Entity dst) {
        static const LaserLine::Line defaultLine = {};

        auto &dstLine = dst.Get<LaserLine>(lock);
        auto *line = std::get_if<LaserLine::Line>(&dstLine.line);
        auto *srcLine = std::get_if<LaserLine::Line>(&src.line);
        if (line && srcLine) {
            if (line->color == defaultLine.color) line->color = srcLine->color;
            if (line->points.empty()) line->points = srcLine->points;
        }
    }
} // namespace ecs
