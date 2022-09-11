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
    const float PLAYER_RADIUS = 0.1f;
    const float PLAYER_CAPSULE_HEIGHT = PLAYER_HEIGHT - 2 * PLAYER_RADIUS;
    const float PLAYER_STEP_HEIGHT = 0.2f;

    struct CharacterController {
        EntityRef target, fallbackTarget, movementProxy;

        physx::PxCapsuleController *pxController = nullptr;
    };

    static Component<CharacterController> ComponenCharacterController("character_controller",
        ComponentField::New("target", &CharacterController::target),
        ComponentField::New("fallback_target", &CharacterController::fallbackTarget),
        ComponentField::New("movement_proxy", &CharacterController::movementProxy));
} // namespace ecs
