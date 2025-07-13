/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/components/Events.hh"

namespace sp {
    class PhysxManager;

    class TriggerSystem {
    public:
        TriggerSystem();
        ~TriggerSystem();

        void Frame(ecs::Lock<ecs::Read<ecs::Name, ecs::TriggerGroup, ecs::TransformSnapshot>,
            ecs::Write<ecs::TriggerArea, ecs::Signals>,
            ecs::SendEventsLock> lock);

        void UpdateEntityTriggers(ecs::Lock<ecs::Read<ecs::Name, ecs::TriggerGroup, ecs::TransformSnapshot>,
                                      ecs::Write<ecs::TriggerArea, ecs::Signals>,
                                      ecs::SendEventsLock> lock,
            ecs::Entity entity);

        ecs::ComponentAddRemoveObserver<ecs::TriggerGroup> triggerGroupObserver;
    };
} // namespace sp
