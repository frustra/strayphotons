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

#include <glm/glm.hpp>

namespace sp {
    namespace gfx {
        struct WinitContext;
    }

    class WinitInputHandler {
    public:
        WinitInputHandler(LockFreeEventQueue<ecs::Event> &windowEventQueue, gfx::WinitContext &context);
        ~WinitInputHandler();

        void Frame();

        static void KeyInputCallback(gfx::WinitContext &context, int key, int scancode, int action, int mods);
        static void CharInputCallback(gfx::WinitContext &context, unsigned int ch);
        static void MouseMoveCallback(gfx::WinitContext &context, double xPos, double yPos);
        static void MouseButtonCallback(gfx::WinitContext &context, int button, int actions, int mods);
        static void MouseScrollCallback(gfx::WinitContext &context, double xOffset, double yOffset);

    private:
        LockFreeEventQueue<ecs::Event> &outputEventQueue;
        gfx::WinitContext &context;

        ecs::EntityRef keyboardEntity = ecs::Name("input", "keyboard");
        ecs::EntityRef mouseEntity = ecs::Name("input", "mouse");
    };
} // namespace sp
