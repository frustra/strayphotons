/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Events.hh"

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
        EntityRef head;

        ecs::EventQueueRef eventQueue;
        physx::PxCapsuleController *pxController = nullptr;
    };

    static Component<CharacterController> ComponentCharacterController({typeid(CharacterController),
        "character_controller",
        "",
        StructField::New("head", &CharacterController::head)});
} // namespace ecs
