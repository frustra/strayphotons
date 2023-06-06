/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "console/CFunc.hh"
#include "core/RegisteredThread.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/components/Events.hh"

namespace ecs {
    class EventQueue;
}

namespace sp {

    class GameLogic : public RegisteredThread {
    public:
        GameLogic(ecs::EventQueue &windowInputQueue, bool stepMode);

        void StartThread();

        static void UpdateInputEvents(const ecs::Lock<ecs::SendEventsLock, ecs::Write<ecs::Signals>> &lock,
            ecs::EventQueue &inputQueue);

    private:
        void Frame() override;

        ecs::EventQueue &windowInputQueue;

        bool stepMode;
        CFuncCollection funcs;
    };

} // namespace sp
