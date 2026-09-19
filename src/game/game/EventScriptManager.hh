/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"
#include "strayphotons/DispatchQueue.hh"
#include "strayphotons/LockFreeEventQueue.hh"

namespace sp {

    class EventScriptManager {
    public:
        EventScriptManager(LockFreeEventQueue<ecs::Event> &windowInputQueue);

        static void UpdateInputEvents(const ecs::Lock<ecs::SendEventsLock, ecs::Write<ecs::Signals>> &lock,
            LockFreeEventQueue<ecs::Event> &inputQueue);

    private:
        LockFreeEventQueue<ecs::Event> &windowInputQueue;
        sp::DispatchQueue workQueue;
    };

} // namespace sp
