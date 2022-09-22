#pragma once

#include "ecs/components/Transform.h"

#include <functional>
#include <glm/glm.hpp>

namespace ecs {
    struct SceneProperties {
        Transform gravityTransform = Transform();
        glm::vec3 fixedGravity = glm::vec3(0, -9.81, 0);
        std::function<glm::vec3(glm::vec3)> gravityFunction;

        glm::vec3 GetGravity(glm::vec3 worldPosition) const {
            if (gravityFunction) {
                auto gravityPos = gravityTransform.GetInverse() * glm::vec4(worldPosition, 1);
                return fixedGravity + gravityTransform.GetRotation() * gravityFunction(gravityPos);
            }
            return fixedGravity;
        }
    };
} // namespace ecs
