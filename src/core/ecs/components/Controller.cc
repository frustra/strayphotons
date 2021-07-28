#include "Controller.hh"

#include <cmath> // For M_PI
#include <limits>

namespace ecs {
    void HumanController::SetRotate(const glm::quat &rotation) {
        std::cout << "Before " << pitch;
        pitch = glm::pitch(rotation);
        std::cout << " After " << pitch;
        if (pitch > M_PI) pitch -= M_PI * 2;
        std::cout << " Fixed " << pitch << std::endl;

        yaw = glm::yaw(rotation);

        if (std::abs(glm::roll(rotation)) > std::numeric_limits<float>::epsilon()) {
            pitch += (pitch > 0) ? -M_PI : M_PI;
            yaw = M_PI - yaw;
        }
    }
} // namespace ecs
