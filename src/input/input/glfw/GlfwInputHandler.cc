#include "GlfwInputHandler.hh"

#include "core/Common.hh"
#include "core/Console.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/core/GraphicsContext.hh"
#include "input/core/BindingNames.hh"
#include "input/core/KeyCodes.hh"
#include "input/glfw/GlfwKeyCodes.hh"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <stdexcept>

namespace sp {
    GlfwInputHandler::GlfwInputHandler(GLFWwindow &glfwWindow) : window(&glfwWindow) {
        glfwSetWindowUserPointer(window, this);

        glfwSetKeyCallback(window, KeyInputCallback);
        glfwSetCharCallback(window, CharInputCallback);
        glfwSetScrollCallback(window, MouseScrollCallback);
        glfwSetMouseButtonCallback(window, MouseButtonCallback);
        glfwSetCursorPosCallback(window, MouseMoveCallback);

        keyboardEntity = ecs::NamedEntity("keyboard");
        mouseEntity = ecs::NamedEntity("mouse");

        UpdateEntities();
    }

    GlfwInputHandler::~GlfwInputHandler() {
        glfwSetKeyCallback(window, nullptr);
        glfwSetCharCallback(window, nullptr);
        glfwSetScrollCallback(window, nullptr);
        glfwSetMouseButtonCallback(window, nullptr);
        glfwSetCursorPosCallback(window, nullptr);

        glfwSetWindowUserPointer(window, nullptr);
    }

    void GlfwInputHandler::UpdateEntities() {
        Logf("Updating keyboard/mouse entities");
        auto lock = ecs::World.StartTransaction<ecs::AddRemove>();

        auto keyboard = keyboardEntity.Get(lock);
        if (!keyboard) {
            Logf("Creating new keyboard entity");
            keyboard = lock.NewEntity();
            keyboard.Set<ecs::Owner>(lock, ecs::Owner::SystemId::INPUT_MANAGER);
            keyboard.Set<ecs::Name>(lock, "keyboard");
            keyboardEntity = ecs::NamedEntity("keyboard", keyboard);
        }
        keyboard.Get<ecs::EventBindings>(lock);
        keyboard.Get<ecs::SignalOutput>(lock);

        auto mouse = mouseEntity.Get(lock);
        if (!mouse) {
            Logf("Creating new mouse entity");
            mouse = lock.NewEntity();
            mouse.Set<ecs::Owner>(lock, ecs::Owner::SystemId::INPUT_MANAGER);
            mouse.Set<ecs::Name>(lock, "mouse");
            mouseEntity = ecs::NamedEntity("mouse", mouse);
        }
        mouse.Get<ecs::EventBindings>(lock);
        mouse.Get<ecs::SignalOutput>(lock);
    }

    void GlfwInputHandler::Frame() {
        bool updateRequired = false;
        {
            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::EventBindings, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>,
                ecs::Write<ecs::EventInput, ecs::SignalOutput>>();
            auto keyboard = keyboardEntity.Get(lock);
            auto mouse = mouseEntity.Get(lock);
            updateRequired = !keyboard.Has<ecs::EventBindings, ecs::SignalOutput>(lock) ||
                             !mouse.Has<ecs::EventBindings, ecs::SignalOutput>(lock);

            frameLock = &lock;
            glfwPollEvents();
            frameLock = nullptr;
        }
        if (updateRequired) UpdateEntities();
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
        if (key == GLFW_KEY_UNKNOWN) return;

        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "KeyInputCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto keyCode = GlfwKeyMapping.find(key);
        Assert(keyCode != GlfwKeyMapping.end(), "Unknown glfw keycode mapping " + std::to_string(key));

        auto keyboard = ctx->keyboardEntity.Get(lock);
        if (action == GLFW_PRESS) {
            if (keyboard.Has<ecs::EventBindings>(lock)) {
                std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + KeycodeNameLookup.at(keyCode->second);
                auto &bindings = keyboard.Get<ecs::EventBindings>(lock);
                bindings.SendEvent(lock, eventName, ctx->keyboardEntity, true);
            }
        }

        if (keyboard.Has<ecs::SignalOutput>(lock)) {
            std::string signalName = INPUT_SIGNAL_KEYBOARD_KEY_BASE + KeycodeNameLookup.at(keyCode->second);
            auto &signalOutput = keyboard.Get<ecs::SignalOutput>(lock);
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
    }

    void GlfwInputHandler::CharInputCallback(GLFWwindow *window, unsigned int ch) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "CharInputCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto keyboard = ctx->keyboardEntity.Get(lock);
        if (keyboard.Has<ecs::EventBindings>(lock)) {
            auto &bindings = keyboard.Get<ecs::EventBindings>(lock);
            // TODO: Handle unicode somehow?
            bindings.SendEvent(lock, INPUT_EVENT_KEYBOARD_CHARACTERS, ctx->keyboardEntity, (char)ch);
        }
    }

    void GlfwInputHandler::MouseMoveCallback(GLFWwindow *window, double xPos, double yPos) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "MouseMoveCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto mouse = ctx->mouseEntity.Get(lock);
        if (mouse.Has<ecs::EventBindings>(lock)) {
            auto &bindings = mouse.Get<ecs::EventBindings>(lock);
            glm::vec2 mousePos(xPos, yPos);
            bindings.SendEvent(lock, INPUT_EVENT_MOUSE_MOVE, ctx->mouseEntity, mousePos - ctx->prevMousePos);
            ctx->prevMousePos = mousePos;
        }

        if (mouse.Has<ecs::SignalOutput>(lock)) {
            auto &signalOutput = mouse.Get<ecs::SignalOutput>(lock);
            signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_CURSOR_X, xPos);
            signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_CURSOR_Y, yPos);
        }
    }

    void GlfwInputHandler::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "MouseButtonCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto mouse = ctx->mouseEntity.Get(lock);
        if (mouse.Has<ecs::EventBindings>(lock)) {
            auto &bindings = mouse.Get<ecs::EventBindings>(lock);
            bindings.SendEvent(lock, INPUT_EVENT_MOUSE_CLICK, ctx->mouseEntity, ctx->ImmediateCursor());
        }

        if (mouse.Has<ecs::SignalOutput>(lock)) {
            std::string signalName;
            if (button == GLFW_MOUSE_BUTTON_LEFT) {
                signalName = INPUT_SIGNAL_MOUSE_BUTTON_LEFT;
            } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
                signalName = INPUT_SIGNAL_MOUSE_BUTTON_MIDDLE;
            } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
                signalName = INPUT_SIGNAL_MOUSE_BUTTON_RIGHT;
            }

            auto &signalOutput = mouse.Get<ecs::SignalOutput>(lock);
            if (action == GLFW_PRESS) {
                signalOutput.SetSignal(signalName, 1.0);
            } else {
                signalOutput.ClearSignal(signalName);
            }
        }
    }

    void GlfwInputHandler::MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock != nullptr, "MouseScrollCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto mouse = ctx->mouseEntity.Get(lock);
        if (mouse.Has<ecs::EventBindings>(lock)) {
            auto &bindings = mouse.Get<ecs::EventBindings>(lock);
            bindings.SendEvent(lock, INPUT_EVENT_MOUSE_SCROLL, ctx->mouseEntity, glm::vec2(xOffset, yOffset));
        }
    }
} // namespace sp
