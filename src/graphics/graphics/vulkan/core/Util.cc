/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "graphics/vulkan/core/Util.hh"

namespace sp::vulkan {
    glm::mat4 MakeOrthographicProjection(float left, float right, float bottom, float top, float near, float far) {
        glm::mat4 proj = glm::mat4(0);
        proj[0][0] = 2.0f / (right - left);
        proj[1][1] = 2.0f / (bottom - top);
        proj[2][2] = 1.0f / (near - far);
        proj[3][0] = -(right + left) / (right - left);
        proj[3][1] = -(bottom + top) / (bottom - top);
        proj[3][2] = near / (near - far);
        proj[3][3] = 1.0f;
        return proj;
    }

    glm::mat4 MakeOrthographicProjection(YDirection yDir, const vk::Rect2D &viewport, float near, float far) {
        float left = viewport.offset.x;
        float right = left + viewport.extent.width;
        float top, bottom;
        if (yDir == YDirection::Up) {
            // OpenGL style
            bottom = viewport.offset.y;
            top = bottom + viewport.extent.height;
        } else {
            top = viewport.offset.y;
            bottom = top + viewport.extent.height;
        }
        return MakeOrthographicProjection(left, right, bottom, top, near, far);
    }

    glm::mat4 MakeOrthographicProjection(YDirection yDir,
        const vk::Rect2D &viewport,
        const glm::vec2 &scale,
        float near,
        float far) {
        float left = viewport.offset.x;
        float right = left + viewport.extent.width;
        float top, bottom;
        if (yDir == YDirection::Up) {
            // OpenGL style
            bottom = viewport.offset.y;
            top = bottom + viewport.extent.height;
        } else {
            top = viewport.offset.y;
            bottom = top + viewport.extent.height;
        }
        return MakeOrthographicProjection(left / scale.x, right / scale.x, bottom / scale.y, top / scale.y, near, far);
    }
} // namespace sp::vulkan
