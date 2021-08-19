#pragma once

#include "ecs/Ecs.hh"

#include <glm/glm.hpp>

struct GLFWwindow;

namespace sp {
    static const std::string INPUT_EVENT_KEYBOARD_KEY = "/keyboard/key"; // int
    static const std::string INPUT_EVENT_KEYBOARD_CHARACTERS = "/keyboard/characters"; // char
    static const std::string INPUT_EVENT_MOUSE_CLICK = "/mouse/click"; // glm::vec2?
    static const std::string INPUT_EVENT_MOUSE_SCROLL = "/mouse/scroll"; // glm::vec2

    static const std::string INPUT_SIGNAL_KEYBOARD_KEY = "keyboard_key";
    static const std::string INPUT_SIGNAL_MOUSE_BUTTON_LEFT = "mouse_button_left";
    static const std::string INPUT_SIGNAL_MOUSE_BUTTON_MIDDLE = "mouse_button_middle";
    static const std::string INPUT_SIGNAL_MOUSE_BUTTON_RIGHT = "mouse_button_right";
    static const std::string INPUT_SIGNAL_MOUSE_CURSOR_X = "mouse_cursor_x";
    static const std::string INPUT_SIGNAL_MOUSE_CURSOR_Y = "mouse_cursor_y";

    class GlfwInputHandler {
    public:
        GlfwInputHandler(GLFWwindow &window);
        ~GlfwInputHandler() {}

        void Frame();

        glm::vec2 ImmediateCursor() const;

        static void KeyInputCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

        static void CharInputCallback(GLFWwindow *window, unsigned int ch);

        static void MouseMoveCallback(GLFWwindow *window, double xPos, double yPos);

        static void MouseButtonCallback(GLFWwindow *window, int button, int actions, int mods);

        static void MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset);

    private:
        GLFWwindow *window = nullptr;
        Tecs::Entity keyboardEntity;
        Tecs::Entity mouseEntity;

        // clang-format off
        ecs::Lock<ecs::Read<ecs::Name, ecs::EventBindings, ecs::SignalBindings>,
                  ecs::Write<ecs::EventInput, ecs::SignalOutput>> *frameLock = nullptr;
        // clang-format on
    };
} // namespace sp
