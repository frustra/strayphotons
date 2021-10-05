#pragma once

#include "core/Config.hh"
#include "ecs/Ecs.hh"
#include "ecs/components/View.hh"

#include <glm/glm.hpp>

namespace sp::vulkan {
    struct GPULight {
        glm::vec3 position;
        float spotAngleCos;

        glm::vec3 tint;
        float intensity;

        glm::vec3 direction;
        float illuminance;

        glm::mat4 proj;
        glm::mat4 invProj;
        glm::mat4 view;
        glm::vec4 mapOffset;
        glm::vec2 clip;
        int gelId;
        float padding[1];
    };

    static_assert(sizeof(GPULight) == 17 * 4 * sizeof(float), "GPULight size incorrect");
} // namespace sp::vulkan
