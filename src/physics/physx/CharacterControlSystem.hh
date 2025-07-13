/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/SignalExpression.hh"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace sp {
    class PhysxManager;

    class CharacterControlSystem {
    public:
        CharacterControlSystem(PhysxManager &manager);
        ~CharacterControlSystem() {}

        void RegisterEvents();
        void Frame(ecs::Lock<ecs::ReadSignalsLock,
            ecs::Read<ecs::EventInput, ecs::SceneProperties>,
            ecs::Write<ecs::TransformTree, ecs::CharacterController>> lock);

    private:
        PhysxManager &manager;
        ecs::ComponentAddRemoveObserver<ecs::CharacterController> characterControllerObserver;
    };
} // namespace sp
