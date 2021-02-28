#include "ecs/components/Controller.hh"
#include "core/Math.h"

#include <limits>



namespace ecs {
    void HumanController::SetRotate(const glm::quat &rotation) {
        pitch = glm::pitch(rotation);
        if (pitch > sp::math::kPi) pitch -= sp::math::kPi * 2;
        yaw = glm::yaw(rotation);

        if (std::abs(glm::roll(rotation)) > std::numeric_limits<float>::epsilon()) {
            pitch += (pitch > 0) ? -sp::math::kPi : sp::math::kPi;
            yaw = sp::math::kPi - yaw;
        }
    }
} // namespace ecs