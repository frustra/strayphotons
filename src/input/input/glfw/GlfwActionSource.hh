#pragma once

#include "input/ActionSource.hh"
#include "input/InputManager.hh"

#include <glm/glm.hpp>
#include <mutex>
#include <queue>
#include <robin_hood.h>

struct GLFWwindow;

namespace sp {
    /**
     * Glfw input source. Provides mouse and keyboard actions.
     */
    class GlfwActionSource final : public ActionSource {
    public:
        GlfwActionSource(InputManager &inputManager, GLFWwindow &window);
        ~GlfwActionSource() {}

        /**
         * Saves the current cursor, scroll, and key values. These will be the
         * values that are retrieved until the next frame.
         */
        void BeginFrame() override;

        /**
         * Add extra handling for binding individual keys
         */
        void BindAction(std::string action, std::string source) override;
        void UnbindAction(std::string action) override;

        /**
         * Returns the x,y position of the current cursor, even if it has moved since the start of frame.
         */
        glm::vec2 ImmediateCursor() const;

        static void KeyInputCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

        static void CharInputCallback(GLFWwindow *window, unsigned int ch);

        static void MouseMoveCallback(GLFWwindow *window, double xPos, double yPos);

        static void MouseButtonCallback(GLFWwindow *window, int button, int actions, int mods);

        static void MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset);

    private:
        GLFWwindow *window = nullptr;
        robin_hood::unordered_flat_map<int, robin_hood::unordered_flat_set<std::string>> keyBindings;

        std::mutex dataLock;
        glm::vec2 mousePos, mouseScroll;
        CharEvents charEvents, charEventsNext;
        KeyEvents keyEvents, keyEventsNext;
        ClickEvents clickEvents, clickEventsNext;
    };
} // namespace sp
