#include "GlfwInputHandler.hh"

#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "graphics/core/GraphicsContext.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"
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

        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "input",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto keyboard = scene->NewSystemEntity(lock, scene, keyboardEntity.Name());
                keyboard.Set<ecs::EventBindings>(lock);
                keyboard.Set<ecs::SignalOutput>(lock);

                auto mouse = scene->NewSystemEntity(lock, scene, mouseEntity.Name());
                mouse.Set<ecs::EventBindings>(lock);
                mouse.Set<ecs::SignalOutput>(lock);
            });
    }

    GlfwInputHandler::~GlfwInputHandler() {
        glfwSetKeyCallback(window, nullptr);
        glfwSetCharCallback(window, nullptr);
        glfwSetScrollCallback(window, nullptr);
        glfwSetMouseButtonCallback(window, nullptr);
        glfwSetCursorPosCallback(window, nullptr);

        glfwSetWindowUserPointer(window, nullptr);
    }

    void GlfwInputHandler::Frame() {
        ZoneScoped;
        {
            auto lock =
                ecs::World.StartTransaction<ecs::ReadSignalsLock, ecs::Write<ecs::SignalOutput>, ecs::SendEventsLock>();
            frameLock = &lock;
            glfwPollEvents();
            frameLock = nullptr;
        }
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
        Assert(ctx->frameLock, "KeyInputCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto keyCode = GlfwKeyMapping.find(key);
        Assertf(keyCode != GlfwKeyMapping.end(), "Unknown glfw keycode mapping %d", key);

        auto keyboard = ctx->keyboardEntity.Get(lock);
        std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + KeycodeNameLookup.at(keyCode->second);
        if (action == GLFW_PRESS) {
            ecs::EventBindings::SendEvent(lock, eventName, keyboard, true);
        } else if (action == GLFW_RELEASE) {
            // TODO: Set up event filters so we can support key-up events
            // ecs::EventBindings::SendEvent(lock, eventName, keyboard, false);
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
        Assert(ctx->frameLock, "CharInputCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto keyboard = ctx->keyboardEntity.Get(lock);
        // TODO: Handle unicode somehow?
        ecs::EventBindings::SendEvent(lock, INPUT_EVENT_KEYBOARD_CHARACTERS, keyboard, (char)ch);
    }

    void GlfwInputHandler::MouseMoveCallback(GLFWwindow *window, double xPos, double yPos) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock, "MouseMoveCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto mouse = ctx->mouseEntity.Get(lock);
        glm::vec2 mousePos(xPos, yPos);
        ecs::EventBindings::SendEvent(lock, INPUT_EVENT_MOUSE_MOVE, mouse, mousePos - ctx->prevMousePos);
        ctx->prevMousePos = mousePos;

        if (mouse.Has<ecs::SignalOutput>(lock)) {
            auto &signalOutput = mouse.Get<ecs::SignalOutput>(lock);
            signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_CURSOR_X, xPos);
            signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_CURSOR_Y, yPos);
        }
    }

    void GlfwInputHandler::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx->frameLock, "MouseButtonCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto mouse = ctx->mouseEntity.Get(lock);

        ecs::EventBindings::SendEvent(lock, INPUT_EVENT_MOUSE_CLICK, mouse, action == GLFW_PRESS);

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
        Assert(ctx->frameLock, "MouseScrollCallback occured without an ECS lock");
        auto &lock = *ctx->frameLock;

        auto mouse = ctx->mouseEntity.Get(lock);
        ecs::EventBindings::SendEvent(lock, INPUT_EVENT_MOUSE_SCROLL, mouse, glm::vec2(xOffset, yOffset));
    }
} // namespace sp
