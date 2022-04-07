#pragma once

#include "ecs/Components.hh"
#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct Light {
        float spotAngle, intensity, illuminance;
        glm::vec3 tint;
        string gelName;
        bool on = true;
        uint32_t shadowMapSize = 9; // shadow map will have a width and height of 2^shadowMapSize
        glm::vec2 shadowMapClip = {0.1, 256}; // near and far plane
    };

    static Component<Light> ComponentLight("light");

    template<>
    bool Component<Light>::Load(ScenePtr scenePtr, const Name &scope, Light &dst, const picojson::value &src);
} // namespace ecs
