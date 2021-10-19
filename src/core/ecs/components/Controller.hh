#pragma once

#include "ecs/Components.hh"
#include "ecs/NamedEntity.hh"

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
    };

    struct CharacterController {
        ecs::NamedEntity target;

        physx::PxCapsuleController *pxController = nullptr;
    };

    static Component<HumanController> ComponentHumanController("human_controller");
    static Component<CharacterController> ComponenCharacterController("character_controller");

    template<>
    bool Component<HumanController>::Load(Lock<Read<ecs::Name>> lock, HumanController &dst, const picojson::value &src);
    template<>
    bool Component<CharacterController>::Load(Lock<Read<ecs::Name>> lock,
        CharacterController &dst,
        const picojson::value &src);
} // namespace ecs
