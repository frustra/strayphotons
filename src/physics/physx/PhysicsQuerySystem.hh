/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class PhysicsQuerySystem {
    public:
        PhysicsQuerySystem(PhysxManager &manager);
        ~PhysicsQuerySystem() {}

        void Frame(ecs::Lock<ecs::Read<ecs::TransformSnapshot>, ecs::Write<ecs::PhysicsQuery>> lock);

    private:
        PhysxManager &manager;
    };
} // namespace sp
