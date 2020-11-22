#define _USE_MATH_DEFINES

#include "ecs/components/Controller.hh"

#include <limits>

namespace ecs {
    void HumanController::SetRotate(const glm::quat &rotation) {
        pitch = glm::pitch(rotation);
        if (pitch > M_PI) pitch -= M_PI * 2;
        yaw = glm::yaw(rotation);

        if (std::abs(glm::roll(rotation)) > std::numeric_limits<float>::epsilon()) {
            pitch += (pitch > 0) ? -M_PI : M_PI;
            yaw = M_PI - yaw;
        }
    }
} // namespace ecs