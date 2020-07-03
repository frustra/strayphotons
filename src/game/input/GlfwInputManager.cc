#include <graphics/GraphicsManager.hh>

#include "GlfwInputManager.hh"
#include <Common.hh>

#include <algorithm>
#include <stdexcept>
#include <core/Logging.hh>
#include <core/Console.hh>

// clang-format off
// GLFW must be included after glew.h (Graphics.hh)
#include "GlfwBindingNames.hh"
#include <GLFW/glfw3.h>
// clang-format on

namespace sp
{
    bool GlfwInputManager::GetActionStateValue(std::string actionPath, bool &value) const
    {
        auto it = actionStatesBoolCurrent.find(actionPath);
        if (it != actionStatesBoolCurrent.end())
        {
            value = it->second;
            return true;
        }
        return false;
    }

    bool GlfwInputManager::GetActionStateValue(std::string actionPath, glm::vec2 &value) const
    {
        auto it = actionStatesVec2Current.find(actionPath);
        if (it != actionStatesVec2Current.end())
        {
            value = it->second;
            return true;
        }
        return false;
    }

    bool GlfwInputManager::GetActionStateDelta(std::string actionPath, bool &value, bool &delta) const
    {
        auto it = actionStatesBoolCurrent.find(actionPath);
        if (it != actionStatesBoolCurrent.end())
        {
            value = it->second;

            auto it2 = actionStatesBoolPrevious.find(actionPath);
            if (it2 != actionStatesBoolPrevious.end())
            {
                delta = it->second != it2->second;
            }
            else
            {
                delta = it->second;
            }
            return true;
        }
        return false;
    }

    bool GlfwInputManager::GetActionStateDelta(std::string actionPath, glm::vec2 &value, glm::vec2 &delta) const
    {
        auto it = actionStatesVec2Current.find(actionPath);
        if (it != actionStatesVec2Current.end())
        {
            value = it->second;

            auto it2 = actionStatesVec2Previous.find(actionPath);
            if (it2 != actionStatesVec2Previous.end())
            {
                delta = it->second - it2->second;
            }
            else
            {
                delta = it->second;
            }
            return true;
        }
        return false;
    }

    template<class T>
    void CopyActionState(T &actionStates, string actionPath, string aliasPath)
    {
        auto it = actionStates.find(actionPath);
        if (it != actionStates.end())
        {
            actionStates[aliasPath] = it->second;
        }
    }

    void GlfwInputManager::BeginFrame()
    {
        // Advance input action snapshots 1 frame.
        actionStatesBoolPrevious = actionStatesBoolCurrent;
        actionStatesVec2Previous = actionStatesVec2Current;
        actionStatesBoolCurrent = actionStatesBoolNext;
        actionStatesVec2Current = actionStatesVec2Next;

        glfwPollEvents();
        
        // Update any action bindings
        for (auto [aliasPath, actionPath] : actionBindings)
        {
            CopyActionState(actionStatesBoolPrevious, actionPath, aliasPath);
            CopyActionState(actionStatesVec2Previous, actionPath, aliasPath);
            CopyActionState(actionStatesBoolCurrent, actionPath, aliasPath);
            CopyActionState(actionStatesVec2Current, actionPath, aliasPath);
        }

        // Run any command bindings
        for (auto [actionPath, command] : commandBindings)
        {
            bool value = false;
            bool changed = false;
            if (GetActionStateDelta(actionPath, value, changed))
            {
                if (value && changed)
                {
                    // Run the bound command
                    GetConsoleManager().QueueParseAndExecute(command);
                }
            }
        }
    }

    glm::vec2 GlfwInputManager::ImmediateCursor() const
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

    void GlfwInputManager::KeyInputCallback(
        GLFWwindow *window,
        int key,
        int scancode,
        int action,
        int mods)
    {
        bool value = false;
        switch (action)
        {
            case GLFW_PRESS:
            case GLFW_REPEAT:
                value = true;
                break;
            case GLFW_RELEASE:
                value = false;
                break;
        }

        auto im = static_cast<GlfwInputManager *>(glfwGetWindowUserPointer(window));
        std::string actionPath;
        if (ActionPathFromGlfwKey(key, actionPath))
        {
            auto it = im->actionStatesBoolNext.find(actionPath);
            if (it != im->actionStatesBoolNext.end())
            {
                it->second = value;
            }
            else
            {
                im->actionStatesBoolNext[actionPath] = value;
            }
        }

        for (auto &cb : im->keyEventCallbacks)
        {
            cb(key, action);
        }
    }

    void GlfwInputManager::CharInputCallback(
        GLFWwindow *window,
        unsigned int ch)
    {
        auto im = static_cast<GlfwInputManager *>(glfwGetWindowUserPointer(window));

        for (auto &cb : im->charEventCallbacks)
        {
            cb(ch);
        }
    }

    void GlfwInputManager::MouseMoveCallback(
        GLFWwindow *window,
        double xPos,
        double yPos)
    {
        auto im = static_cast<GlfwInputManager *>(glfwGetWindowUserPointer(window));
        auto it = im->actionStatesVec2Next.find(INPUT_ACTION_MOUSE_CURSOR);
        if (it != im->actionStatesVec2Next.end())
        {
            it->second.x = (float) xPos;
            it->second.y = (float) yPos;
        }
        else
        {
            im->actionStatesVec2Next[INPUT_ACTION_MOUSE_CURSOR] = glm::vec2((float) xPos, (float) yPos);
        }
    }

    void GlfwInputManager::MouseButtonCallback(
        GLFWwindow *window,
        int button,
        int action,
        int mods)
    {
        bool value = false;
        switch (action)
        {
            case GLFW_PRESS:
                value = true;
                break;
            case GLFW_RELEASE:
                value = false;
                break;
        }

        auto im = static_cast<GlfwInputManager *>(glfwGetWindowUserPointer(window));
        std::string actionPath;
        if (ActionPathFromGlfwMouseButton(button, actionPath))
        {
            auto it = im->actionStatesBoolNext.find(actionPath);
            if (it != im->actionStatesBoolNext.end())
            {
                it->second = value;
            }
            else
            {
                im->actionStatesBoolNext[actionPath] = value;
            }
        }
    }

    void GlfwInputManager::MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset)
    {
        auto im = static_cast<GlfwInputManager *>(glfwGetWindowUserPointer(window));
        auto it = im->actionStatesVec2Next.find(INPUT_ACTION_MOUSE_SCROLL);
        if (it != im->actionStatesVec2Next.end())
        {
            it->second.x += (float) xOffset;
            it->second.y += (float) yOffset;
        }
        else
        {
            im->actionStatesVec2Next[INPUT_ACTION_MOUSE_SCROLL] = glm::vec2((float) xOffset, (float) xOffset);
        }
    }

    void GlfwInputManager::BindCallbacks(GLFWwindow *window)
    {
        this->window = window;

        DisableCursor();

        // store a pointer to this GlfwInputManager since we must provide static functions
        glfwSetWindowUserPointer(window, this);

        glfwSetKeyCallback(window, KeyInputCallback);
        glfwSetCharCallback(window, CharInputCallback);
        glfwSetScrollCallback(window, MouseScrollCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwSetCursorPosCallback(window, MouseMoveCallback);
    }

    void GlfwInputManager::DisableCursor()
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void GlfwInputManager::EnableCursor()
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}
