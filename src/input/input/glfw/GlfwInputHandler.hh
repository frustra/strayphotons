#pragma once

#include "ecs/Ecs.hh"
#include "ecs/NamedEntity.hh"

#include <glm/glm.hpp>

struct GLFWwindow;

namespace sp {
    class GlfwInputHandler {
    public:
        GlfwInputHandler(GLFWwindow &window);
        ~GlfwInputHandler() {}

        void UpdateEntities();

        void Frame();

        glm::vec2 ImmediateCursor() const;

        static void KeyInputCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

        static void CharInputCallback(GLFWwindow *window, unsigned int ch);

        static void MouseMoveCallback(GLFWwindow *window, double xPos, double yPos);

        static void MouseButtonCallback(GLFWwindow *window, int button, int actions, int mods);

        static void MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset);

    private:
        GLFWwindow *window = nullptr;

        ecs::NamedEntity keyboardEntity;
        ecs::NamedEntity mouseEntity;

        glm::vec2 prevMousePos;

        // clang-format off
        ecs::Lock<ecs::Read<ecs::Name, ecs::EventBindings, ecs::SignalBindings>,
                  ecs::Write<ecs::EventInput, ecs::SignalOutput>> *frameLock = nullptr;
        // clang-format on
    };
} // namespace sp
