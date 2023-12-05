/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "console/CFunc.hh"
#include "core/LockFreeEventQueue.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Events.hh"

namespace sp {

    class GameLogic : public RegisteredThread {
    public:
        GameLogic(LockFreeEventQueue<ecs::Event> &windowInputQueue);

        void StartThread(bool startPaused = false);

        static void UpdateInputEvents(const ecs::Lock<ecs::SendEventsLock, ecs::Write<ecs::Signals>> &lock,
            LockFreeEventQueue<ecs::Event> &inputQueue);

    private:
        void Frame() override;

        LockFreeEventQueue<ecs::Event> &windowInputQueue;

        bool stepMode;
        CFuncCollection funcs;
    };

} // namespace sp
