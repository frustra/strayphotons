#include <graphics/GraphicsManager.hh>

#include "GlfwActionSource.hh"
#include <Common.hh>

#include <algorithm>
#include <stdexcept>
#include <GLFW/glfw3.h>
#include <core/Logging.hh>
#include <core/Console.hh>

namespace sp
{
    GlfwActionSource::GlfwActionSource(InputManager &inputManager, GLFWwindow &glfwWindow)
        : ActionSource(inputManager), window(&glfwWindow)
    {
        DisableCursor();

        // store a pointer to this GlfwInputManager since we must provide static functions
        glfwSetWindowUserPointer(window, this);

        glfwSetKeyCallback(window, KeyInputCallback);
        glfwSetCharCallback(window, CharInputCallback);
        glfwSetScrollCallback(window, MouseScrollCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwSetCursorPosCallback(window, MouseMoveCallback);
    }

    void GlfwActionSource::BeginFrame()
    {
        glfwPollEvents(); // TODO: Move this to its own thread.

        std::lock_guard lock(dataLock);
        SetAction(INPUT_ACTION_MOUSE_CURSOR, &mousePos);
        SetAction(INPUT_ACTION_MOUSE_SCROLL, &mouseScroll);

        for (auto &click : clickEventsNext)
        {
            if (click.button == GLFW_MOUSE_BUTTON_LEFT)
            {
                SetAction(INPUT_ACTION_MOUSE_BUTTON_LEFT, &click.down);
            }
            else if (click.button == GLFW_MOUSE_BUTTON_MIDDLE)
            {
                SetAction(INPUT_ACTION_MOUSE_BUTTON_MIDDLE, &click.down);
            }
            else if (click.button == GLFW_MOUSE_BUTTON_RIGHT)
            {
                SetAction(INPUT_ACTION_MOUSE_BUTTON_RIGHT, &click.down);
            }
        }

        keyEvents = keyEventsNext;
        SetAction(INPUT_ACTION_KEYBOARD_KEYS, &keyEvents);

        charEvents = charEventsNext;
        charEventsNext.clear();
        SetAction(INPUT_ACTION_KEYBOARD_CHARS, &charEvents);

        clickEvents = clickEventsNext;
        clickEventsNext.clear();
        SetAction(INPUT_ACTION_MOUSE_CLICK, &clickEvents);
    }

    glm::vec2 GlfwActionSource::ImmediateCursor() const
    {
        if (glfwGetWindowAttrib(window, GLFW_FOCUSED))
        {
            double mouseX, mouseY;
            int fbWidth, fbHeight;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            glm::ivec2 windowSize = CVarWindowSize.Get();
            return glm::vec2((float) mouseX, (float) mouseY + (float) (windowSize.y - fbHeight));
        }
        else
        {
            return glm::vec2(-1.0f, -1.0f);
        }
    }

    void GlfwActionSource::KeyInputCallback(
        GLFWwindow *window,
        int key,
        int scancode,
        int action,
        int mods)
    {
        auto ctx = static_cast<GlfwActionSource *>(glfwGetWindowUserPointer(window));

        std::lock_guard lock(ctx->dataLock);
        switch (action)
        {
        case GLFW_PRESS:
        case GLFW_REPEAT:
            ctx->keyEventsNext.emplace(key);
            break;
        case GLFW_RELEASE:
            ctx->keyEventsNext.erase(key);
            break;
        }
    }

    void GlfwActionSource::CharInputCallback(
        GLFWwindow *window,
        unsigned int ch)
    {
        auto ctx = static_cast<GlfwActionSource *>(glfwGetWindowUserPointer(window));

        std::lock_guard lock(ctx->dataLock);
        ctx->charEventsNext.emplace_back(ch);
    }

    void GlfwActionSource::MouseMoveCallback(
        GLFWwindow *window,
        double xPos,
        double yPos)
    {
        auto ctx = static_cast<GlfwActionSource *>(glfwGetWindowUserPointer(window));

        std::lock_guard lock(ctx->dataLock);
        ctx->mousePos.x = (float) xPos;
        ctx->mousePos.y = (float) yPos;
    }

    void GlfwActionSource::MouseButtonCallback(
        GLFWwindow *window,
        int button,
        int action,
        int mods)
    {
        auto ctx = static_cast<GlfwActionSource *>(glfwGetWindowUserPointer(window));

        std::lock_guard lock(ctx->dataLock);
        ctx->clickEventsNext.emplace_back(button, ctx->ImmediateCursor(), action == GLFW_PRESS);
    }

    void GlfwActionSource::MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset)
    {
        auto ctx = static_cast<GlfwActionSource *>(glfwGetWindowUserPointer(window));

        std::lock_guard lock(ctx->dataLock);
        ctx->mouseScroll.x = (float) xOffset;
        ctx->mouseScroll.y = (float) yOffset;
    }

    void GlfwActionSource::DisableCursor()
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void GlfwActionSource::EnableCursor()
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}
