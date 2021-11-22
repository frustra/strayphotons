#pragma once

#include "graphics/vulkan/core/Common.hh"

namespace sp::vulkan {
    glm::mat4 MakeOrthographicProjection(float left,
        float right,
        float bottom,
        float top,
        float near = 0.0f,
        float far = 1.0f);
    glm::mat4 MakeOrthographicProjection(const vk::Rect2D &viewport, float near = 0.0f, float far = 1.0f);
} // namespace sp::vulkan
