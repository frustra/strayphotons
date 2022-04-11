#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct LaserEmitter {
        float intensity = 1.0f; // multiplier applied to color
        glm::vec3 color; // HDR value
        bool on = true;
    };

    static Component<LaserEmitter> ComponentLaserEmitter("laser_emitter");

    template<>
    bool Component<LaserEmitter>::Load(const EntityScope &scope, LaserEmitter &dst, const picojson::value &src);
} // namespace ecs
