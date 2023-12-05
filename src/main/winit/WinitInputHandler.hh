/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#ifdef SP_RUST_WINIT_SUPPORT
    #include "InputCallbacks.hh"
    #include "core/LockFreeEventQueue.hh"
    #include "ecs/Ecs.hh"
    #include "ecs/EventQueue.hh"

    #include <glm/glm.hpp>

namespace sp::winit {
    struct WinitContext;
}

namespace sp {
    struct CGameContext;

    class WinitInputHandler {
    public:
        WinitInputHandler(CGameContext *ctx);
        ~WinitInputHandler() {}

        void StartEventLoop(uint32_t maxInputRate);

        CGameContext *ctx;
        LockFreeEventQueue<ecs::Event> &outputEventQueue;
        winit::WinitContext *context = nullptr;

        ecs::Entity mouse, keyboard;
    };
} // namespace sp

#endif
