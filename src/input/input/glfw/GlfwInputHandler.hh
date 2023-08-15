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

struct GLFWwindow;

namespace sp {
    class GlfwInputHandler {
    public:
        GlfwInputHandler(LockFreeEventQueue<ecs::Event> &windowEventQueue, GLFWwindow &window);
        ~GlfwInputHandler();

        void Frame();

        static void KeyInputCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
        static void CharInputCallback(GLFWwindow *window, unsigned int ch);
        static void MouseMoveCallback(GLFWwindow *window, double xPos, double yPos);
        static void MouseButtonCallback(GLFWwindow *window, int button, int actions, int mods);
        static void MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset);

    private:
        LockFreeEventQueue<ecs::Event> &outputEventQueue;
        GLFWwindow *window = nullptr;

        glm::vec2 prevMousePos = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
        ecs::EntityRef keyboardEntity = ecs::Name("input", "keyboard");
        ecs::EntityRef mouseEntity = ecs::Name("input", "mouse");
    };
} // namespace sp
