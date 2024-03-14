/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#ifdef SP_RUST_WINIT_SUPPORT
    #include <glm/glm.hpp>
    #include <strayphotons.h>

namespace sp::winit {
    struct WinitContext;
}

namespace sp {
    class WinitInputHandler {
    public:
        WinitInputHandler(sp_game_t ctx, winit::WinitContext *winitContext);
        ~WinitInputHandler() {}

        void StartEventLoop(uint32_t maxInputRate);

        sp_game_t ctx;
        winit::WinitContext *winitContext = nullptr;

        sp_entity_t mouse, keyboard;
    };
} // namespace sp

#endif
