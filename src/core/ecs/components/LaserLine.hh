#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct LaserLine {
        float intensity = 1.0f; // multiplier applied to color
        glm::vec3 color; // HDR value
        vector<glm::vec3> points;
        bool on = true;
        bool relative = true; // multiply transform
        float radius = 0.003; // in world units
    };

    static Component<LaserLine> ComponentLaserLine("laser_line");

    template<>
    bool Component<LaserLine>::Load(sp::Scene *scene, LaserLine &dst, const picojson::value &src);
} // namespace ecs
