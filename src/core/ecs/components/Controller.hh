#pragma once

#include <ecs/Components.hh>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace physx {
    class PxCapsuleController;
}

namespace ecs {
    // Units in meters
    const float PLAYER_HEIGHT = 1.7f;
    const float PLAYER_CROUCH_HEIGHT = 0.8f;
    const float PLAYER_RADIUS = 0.2f;
    const float PLAYER_CAPSULE_CROUCH_HEIGHT = PLAYER_CROUCH_HEIGHT - 2 * PLAYER_RADIUS;
    const float PLAYER_CAPSULE_HEIGHT = PLAYER_HEIGHT - 2 * PLAYER_RADIUS;
    const float PLAYER_STEP_HEIGHT = 0.2f;
    const float PLAYER_SWEEP_DISTANCE = 0.4f; // Distance to check if on ground

    const float PLAYER_GRAVITY = 9.81f;
    const float PLAYER_JUMP_VELOCITY = 4.0f;
    const float PLAYER_AIR_STRAFE = 0.8f; // Movement scaler for acceleration in air
    const float PLAYER_PUSH_FORCE = 0.3f;

    class HumanController {
    public:
        // pxCapsuleController handles movement and physx simulation
        physx::PxCapsuleController *pxController;

        float height = ecs::PLAYER_CAPSULE_HEIGHT;

        bool onGround = false;
        glm::vec3 velocity = glm::vec3(0);
    };

    static Component<HumanController> ComponentHumanController("human_controller");
} // namespace ecs
