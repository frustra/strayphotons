/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "graphics/vulkan/core/VkCommon.hh"

namespace sp::vulkan {
    glm::mat4 MakeOrthographicProjection(float left,
        float right,
        float bottom,
        float top,
        float near = 0.0f,
        float far = 1.0f);
    glm::mat4 MakeOrthographicProjection(const vk::Rect2D &viewport, float near = 0.0f, float far = 1.0f);
} // namespace sp::vulkan
