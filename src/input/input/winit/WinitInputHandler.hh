/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "core/LockFreeEventQueue.hh"
#include "ecs/Ecs.hh"
#include "ecs/EntityRef.hh"
#include "ecs/EventQueue.hh"
#include "input/winit/InputCallbacks.hh"

#include <glm/glm.hpp>

namespace sp {
    namespace winit {
        struct WinitContext;
    } // namespace winit

    class GraphicsManager;

    class WinitInputHandler {
    public:
        WinitInputHandler(GraphicsManager &manager,
            LockFreeEventQueue<ecs::Event> &windowEventQueue,
            winit::WinitContext &context);

        void StartEventLoop(uint32_t maxInputRate);

        GraphicsManager &manager;
        LockFreeEventQueue<ecs::Event> &outputEventQueue;
        winit::WinitContext &context;

        ecs::EntityRef keyboardEntity = ecs::Name("input", "keyboard");
        ecs::EntityRef mouseEntity = ecs::Name("input", "mouse");
    };
} // namespace sp
