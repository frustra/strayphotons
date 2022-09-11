#pragma once

#include <functional>
#include <glm/glm.hpp>

namespace ecs {
    struct SceneProperties {
        glm::vec3 fixedGravity = glm::vec3(0, -9.81, 0);
        std::function<glm::vec3(glm::vec3)> gravityFunction;
    };
} // namespace ecs
