#pragma once

#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"

#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace physx {
    class PxCapsuleController;
}

namespace ecs {
    // Units in meters
    const float PLAYER_HEIGHT = 1.8f;
    const float PLAYER_RADIUS = 0.2f;
    const float PLAYER_CAPSULE_HEIGHT = PLAYER_HEIGHT - 2 * PLAYER_RADIUS;
    const float PLAYER_STEP_HEIGHT = 0.05f;

    const float PLAYER_GRAVITY = 9.81f;
    const float PLAYER_JUMP_VELOCITY = 4.0f;
    const float PLAYER_AIR_STRAFE = 0.8f; // Movement scaler for acceleration in air

    struct CharacterController {
        EntityRef target, fallbackTarget, movementProxy;

        physx::PxCapsuleController *pxController = nullptr;
    };

    static Component<CharacterController> ComponenCharacterController("character_controller");

    template<>
    bool Component<CharacterController>::Load(const EntityScope &scope,
        CharacterController &dst,
        const picojson::value &src);
} // namespace ecs
