#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>
#include <variant>

namespace ecs {
    struct LaserLine {
        struct Line {
            std::vector<glm::vec3> points;
            glm::vec3 color = glm::vec3(1); // HDR value
        };
        struct Segment {
            glm::vec3 start, end;
            glm::vec3 color = glm::vec3(1); // HDR value
        };
        using Segments = std::vector<Segment>;
        std::variant<Line, Segments> line = Line();

        float intensity = 1.0f; // multiplier applied to color
        float mediaDensityFactor = 1.0f;
        bool on = true;
        bool relative = true; // multiply transform
        float radius = 0.003; // in world units
    };

    static Component<LaserLine> ComponentLaserLine("laser_line");

    template<>
    bool Component<LaserLine>::Load(const EntityScope &scope, LaserLine &dst, const picojson::value &src);
} // namespace ecs
