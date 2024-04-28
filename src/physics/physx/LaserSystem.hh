/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/SignalExpression.hh"

namespace sp {
    class PhysxManager;

    class LaserSystem {
    public:
        LaserSystem(PhysxManager &manager);
        ~LaserSystem() {}

        void Frame(ecs::Lock<ecs::ReadSignalsLock,
            ecs::Read<ecs::TransformSnapshot, ecs::LaserEmitter, ecs::OpticalElement>,
            ecs::Write<ecs::LaserLine, ecs::LaserSensor, ecs::Signals>> lock);

    private:
        PhysxManager &manager;
    };
} // namespace sp
