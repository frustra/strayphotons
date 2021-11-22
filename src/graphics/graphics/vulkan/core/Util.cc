#include "graphics/vulkan/core/Util.hh"

namespace sp::vulkan {
    glm::mat4 MakeOrthographicProjection(float left, float right, float bottom, float top, float near, float far) {
        glm::mat4 proj;
        proj[0][0] = 2.0f / (right - left);
        proj[1][1] = 2.0f / (bottom - top);
        proj[2][2] = 1.0f / (near - far);
        proj[3][0] = -(right + left) / (right - left);
        proj[3][1] = -(bottom + top) / (bottom - top);
        proj[3][2] = near / (near - far);
        proj[3][3] = 1.0f;
        return proj;
    }
    glm::mat4 MakeOrthographicProjection(const vk::Rect2D &viewport, float near, float far) {
        // Uses OpenGL style, Y up
        float left = viewport.offset.x;
        float right = left + viewport.extent.width;
        float bottom = viewport.offset.y;
        float top = bottom + viewport.extent.height;
        return MakeOrthographicProjection(left, right, bottom, top, near, far);
    }
} // namespace sp::vulkan
