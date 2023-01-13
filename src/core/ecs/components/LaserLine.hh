#pragma once

#include "core/Common.hh"
#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <variant>

namespace ecs {
    struct LaserLine {
        struct Line {
            std::vector<glm::vec3> points;
            sp::color_t color = glm::vec3(1); // HDR value
        };
        struct Segment {
            glm::vec3 start, end;
            sp::color_t color = glm::vec3(1); // HDR value
        };
        using Segments = std::vector<Segment>;
        std::variant<Line, Segments> line = Line();

        float intensity = 1.0f; // multiplier applied to color
        float mediaDensityFactor = 1.0f;
        bool on = true;
        bool relative = true; // multiply transform
        float radius = 0.003; // in world units
    };

    static StructMetadata MetadataLaserLine(typeid(LaserLine),
        StructField::New("intensity", &LaserLine::intensity),
        StructField::New("media_density", &LaserLine::mediaDensityFactor),
        StructField::New("on", &LaserLine::on),
        StructField::New("relative", &LaserLine::relative),
        StructField::New("radius", &LaserLine::radius));
    static Component<LaserLine> ComponentLaserLine("laser_line", MetadataLaserLine);

    template<>
    void StructMetadata::InitUndefined<LaserLine>(LaserLine &dst);
    template<>
    bool StructMetadata::Load<LaserLine>(const EntityScope &scope, LaserLine &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<LaserLine>(const EntityScope &scope,
        picojson::value &dst,
        const LaserLine &src,
        const LaserLine &def);
    template<>
    void Component<LaserLine>::Apply(LaserLine &dst, const LaserLine &src, bool liveTarget);
} // namespace ecs
