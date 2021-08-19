#include "GlfwInputHandler.hh"

#include "core/Common.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/core/GraphicsContext.hh"

#include <algorithm>
#include <glm/glm.hpp>
#include <stdexcept>

// clang-format off
// GLFW must be included after glew.h (Graphics.hh)
#include "GlfwBindingNames.hh"
#include <GLFW/glfw3.h>
// clang-format on

namespace sp {
    GlfwInputHandler::GlfwInputHandler(GLFWwindow &glfwWindow) : window(&glfwWindow) {
        glfwSetWindowUserPointer(window, this);

        glfwSetKeyCallback(window, KeyInputCallback);
        glfwSetCharCallback(window, CharInputCallback);
        glfwSetScrollCallback(window, MouseScrollCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwSetCursorPosCallback(window, MouseMoveCallback);

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            keyboardEntity = lock.NewEntity();
            keyboardEntity.Set<ecs::Owner>(lock, ecs::Owner::SystemId::GLFW_INPUT);
            keyboardEntity.Set<ecs::Name>(lock, "keyboard");
            keyboardEntity.Set<ecs::SignalOutput>(lock);
            mouseEntity = lock.NewEntity();
            mouseEntity.Set<ecs::Owner>(lock, ecs::Owner::SystemId::GLFW_INPUT);
            mouseEntity.Set<ecs::Name>(lock, "mouse");
            mouseEntity.Set<ecs::SignalOutput>(lock);
        }
    }

    void GlfwInputHandler::Frame() {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::EventBindings, ecs::SignalBindings>,
                                                ecs::Write<ecs::EventInput, ecs::SignalOutput>>();
        frameLock = &lock;
        glfwPollEvents();
        frameLock = nullptr;
    }

    glm::vec2 GlfwInputHandler::ImmediateCursor() const {
        if (glfwGetWindowAttrib(window, GLFW_FOCUSED)) {
            double mouseX, mouseY;
            int fbWidth, fbHeight;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            glm::ivec2 windowSize = CVarWindowSize.Get();
            return glm::vec2((float)mouseX, (float)mouseY + (float)(windowSize.y - fbHeight));
        } else {
            return glm::vec2(-1.0f, -1.0f);
        }
    }

    void GlfwInputHandler::KeyInputCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "KeyInputCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        std::string eventName = INPUT_EVENT_KEYBOARD_KEY + "_" + GlfwKeyNames.at(key);
        for (auto entity : lock.EntitiesWith<ecs::EventBindings>()) {
            auto &bindings = entity.Get<ecs::EventBindings>(lock);
            bindings.SendEvent(lock, eventName, ecs::NamedEntity("keyboard", ctx->keyboardEntity), key);
        }

        std::string signalName = INPUT_SIGNAL_KEYBOARD_KEY + "_" + GlfwKeyNames.at(key);
        auto &signalOutput = ctx->keyboardEntity.Get<ecs::SignalOutput>(lock);
        switch (action) {
        case GLFW_PRESS:
        case GLFW_REPEAT:
            signalOutput.SetSignal(signalName, 1.0);
            break;
        case GLFW_RELEASE:
            signalOutput.ClearSignal(signalName);
            break;
        }
    }

    void GlfwInputHandler::CharInputCallback(GLFWwindow *window, unsigned int ch) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "CharInputCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        for (auto entity : lock.EntitiesWith<ecs::EventBindings>()) {
            auto &bindings = entity.Get<ecs::EventBindings>(lock);
            // TODO: Handle unicode somehow?
            bindings.SendEvent(lock,
                               INPUT_EVENT_KEYBOARD_CHARACTERS,
                               ecs::NamedEntity("keyboard", ctx->keyboardEntity),
                               (char)ch);
        }
    }

    void GlfwInputHandler::MouseMoveCallback(GLFWwindow *window, double xPos, double yPos) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "MouseMoveCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto &signalOutput = ctx->mouseEntity.Get<ecs::SignalOutput>(lock);
        signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_CURSOR_X, xPos);
        signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_CURSOR_Y, yPos);
    }

    void GlfwInputHandler::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "MouseButtonCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        for (auto entity : lock.EntitiesWith<ecs::EventBindings>()) {
            auto &bindings = entity.Get<ecs::EventBindings>(lock);
            bindings.SendEvent(lock,
                               INPUT_EVENT_MOUSE_CLICK,
                               ecs::NamedEntity("mouse", ctx->mouseEntity),
                               ctx->ImmediateCursor());
        }

        std::string signalName;
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            signalName = INPUT_SIGNAL_MOUSE_BUTTON_LEFT;
        } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            signalName = INPUT_SIGNAL_MOUSE_BUTTON_MIDDLE;
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            signalName = INPUT_SIGNAL_MOUSE_BUTTON_RIGHT;
        }

        auto &signalOutput = ctx->mouseEntity.Get<ecs::SignalOutput>(lock);
        if (action == GLFW_PRESS) {
            signalOutput.SetSignal(signalName, 1.0);
        } else {
            signalOutput.ClearSignal(signalName);
        }
    }

    void GlfwInputHandler::MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "MouseScrollCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        for (auto entity : lock.EntitiesWith<ecs::EventBindings>()) {
            auto &bindings = entity.Get<ecs::EventBindings>(lock);
            bindings.SendEvent(lock,
                               INPUT_EVENT_MOUSE_SCROLL,
                               ecs::NamedEntity("mouse", ctx->mouseEntity),
                               glm::vec2(xOffset, yOffset));
        }
    }
} // namespace sp
