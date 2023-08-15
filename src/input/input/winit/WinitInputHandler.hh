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

#include <cstdint>
#include <glm/glm.hpp>

namespace sp {
    enum KeyCode : int;
    enum InputAction : int;
    enum MouseButton : int;

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

    bool InputFrameCallback(WinitInputHandler *ctx);
    void KeyInputCallback(WinitInputHandler *ctx, KeyCode key, int scancode, InputAction action);
    void CharInputCallback(WinitInputHandler *ctx, unsigned int ch);
    void MouseMoveCallback(WinitInputHandler *ctx, double dx, double dy);
    void MousePositionCallback(WinitInputHandler *ctx, double xPos, double yPos);
    void MouseButtonCallback(WinitInputHandler *ctx, MouseButton button, InputAction actions);
    void MouseScrollCallback(WinitInputHandler *ctx, double xOffset, double yOffset);
} // namespace sp
