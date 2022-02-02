#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct LaserLine {
        float intensity = 1.0f; // multiplier applied to color
        glm::vec4 color; // rgb is HDR, alpha channel is in [0..1]
        vector<glm::vec3> points;
        bool on = true;
    };

    static Component<LaserLine> ComponentLaserLine("laser_line");

    template<>
    bool Component<LaserLine>::Load(sp::Scene *scene, LaserLine &dst, const picojson::value &src);
} // namespace ecs
