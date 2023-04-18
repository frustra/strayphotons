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
    GlfwInputHandler::GlfwInputHandler(GLFWwindow &glfwWindow) : window(&glfwWindow), glfwEventQueue(1000) {
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
        {
            ZoneScopedN("GlfwPollEvents");
            glfwPollEvents();
        }
        if (!glfwEventQueue.Empty()) {
            ZoneScopedN("GlfwCommitEvents");
            auto lock = ecs::StartTransaction<ecs::SendEventsLock, ecs::Write<ecs::SignalOutput>>();

            auto keyboard = keyboardEntity.Get(lock);
            auto mouse = mouseEntity.Get(lock);

            ecs::Event event;
            while (glfwEventQueue.Poll(event, lock.GetTransactionId())) {
                if (event.source == keyboard) {
                    ecs::EventBindings::SendEvent(lock, keyboardEntity, event);
                } else if (event.source == mouse) {
                    ecs::EventBindings::SendEvent(lock, mouseEntity, event);
                }

                if (event.name == INPUT_EVENT_KEYBOARD_KEY_DOWN) {
                    auto &keyName = std::get<std::string>(event.data);
                    std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName;
                    ecs::EventBindings::SendEvent(lock, keyboardEntity, ecs::Event{eventName, keyboard, true});

                    if (keyboard.Has<ecs::SignalOutput>(lock)) {
                        auto &signalOutput = keyboard.Get<ecs::SignalOutput>(lock);
                        signalOutput.SetSignal(INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName, 1.0);
                    }
                } else if (event.name == INPUT_EVENT_KEYBOARD_KEY_UP) {
                    auto &keyName = std::get<std::string>(event.data);
                    std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName;
                    ecs::EventBindings::SendEvent(lock, keyboardEntity, ecs::Event{eventName, keyboard, false});

                    if (keyboard.Has<ecs::SignalOutput>(lock)) {
                        auto &signalOutput = keyboard.Get<ecs::SignalOutput>(lock);
                        signalOutput.ClearSignal(INPUT_SIGNAL_KEYBOARD_KEY_BASE + keyName);
                    }
                } else if (event.name == INPUT_EVENT_MOUSE_POSITION) {
                    auto &mousePos = std::get<glm::vec2>(event.data);
                    ecs::EventBindings::SendEvent(lock,
                        mouseEntity,
                        ecs::Event{INPUT_EVENT_MOUSE_MOVE, mouse, mousePos - prevMousePos});
                    prevMousePos = mousePos;

                    if (mouse.Has<ecs::SignalOutput>(lock)) {
                        auto &signalOutput = mouse.Get<ecs::SignalOutput>(lock);
                        signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_CURSOR_X, mousePos.x);
                        signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_CURSOR_Y, mousePos.y);
                    }
                } else if (event.name == INPUT_EVENT_MOUSE_LEFT_CLICK) {
                    if (mouse.Has<ecs::SignalOutput>(lock)) {
                        auto &signalOutput = mouse.Get<ecs::SignalOutput>(lock);
                        if (std::get<bool>(event.data)) {
                            signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_BUTTON_LEFT, 1.0);
                        } else {
                            signalOutput.ClearSignal(INPUT_SIGNAL_MOUSE_BUTTON_LEFT);
                        }
                    }
                } else if (event.name == INPUT_EVENT_MOUSE_MIDDLE_CLICK) {
                    if (mouse.Has<ecs::SignalOutput>(lock)) {
                        auto &signalOutput = mouse.Get<ecs::SignalOutput>(lock);
                        if (std::get<bool>(event.data)) {
                            signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_BUTTON_MIDDLE, 1.0);
                        } else {
                            signalOutput.ClearSignal(INPUT_SIGNAL_MOUSE_BUTTON_MIDDLE);
                        }
                    }
                } else if (event.name == INPUT_EVENT_MOUSE_RIGHT_CLICK) {
                    if (mouse.Has<ecs::SignalOutput>(lock)) {
                        auto &signalOutput = mouse.Get<ecs::SignalOutput>(lock);
                        if (std::get<bool>(event.data)) {
                            signalOutput.SetSignal(INPUT_SIGNAL_MOUSE_BUTTON_RIGHT, 1.0);
                        } else {
                            signalOutput.ClearSignal(INPUT_SIGNAL_MOUSE_BUTTON_RIGHT);
                        }
                    }
                }
            }
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
        ZoneScoped;
        if (key == GLFW_KEY_UNKNOWN) return;

        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx, "KeyInputCallback occured without valid context");

        auto keyCode = GlfwKeyMapping.find(key);
        Assertf(keyCode != GlfwKeyMapping.end(), "Unknown glfw keycode mapping %d", key);
        auto keyName = KeycodeNameLookup.at(keyCode->second);

        auto keyboard = ctx->keyboardEntity.GetLive();
        std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName;
        if (action == GLFW_PRESS) {
            ctx->glfwEventQueue.Add(ecs::Event{INPUT_EVENT_KEYBOARD_KEY_DOWN, keyboard, keyName});
        } else if (action == GLFW_RELEASE) {
            ctx->glfwEventQueue.Add(ecs::Event{INPUT_EVENT_KEYBOARD_KEY_UP, keyboard, keyName});
        }
    }

    void GlfwInputHandler::CharInputCallback(GLFWwindow *window, unsigned int ch) {
        ZoneScoped;
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx, "CharInputCallback occured without valid context");

        auto keyboard = ctx->keyboardEntity.GetLive();
        // TODO: Handle unicode somehow?
        ctx->glfwEventQueue.Add(ecs::Event{INPUT_EVENT_KEYBOARD_CHARACTERS, keyboard, (char)ch});
    }

    void GlfwInputHandler::MouseMoveCallback(GLFWwindow *window, double xPos, double yPos) {
        ZoneScoped;
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx, "MouseMoveCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        ctx->glfwEventQueue.Add(ecs::Event{INPUT_EVENT_MOUSE_POSITION, mouse, glm::vec2(xPos, yPos)});
    }

    void GlfwInputHandler::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        ZoneScoped;
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx, "MouseButtonCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            ctx->glfwEventQueue.Add(ecs::Event{INPUT_EVENT_MOUSE_LEFT_CLICK, mouse, action == GLFW_PRESS});
        } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            ctx->glfwEventQueue.Add(ecs::Event{INPUT_EVENT_MOUSE_MIDDLE_CLICK, mouse, action == GLFW_PRESS});
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            ctx->glfwEventQueue.Add(ecs::Event{INPUT_EVENT_MOUSE_RIGHT_CLICK, mouse, action == GLFW_PRESS});
        }
    }

    void GlfwInputHandler::MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
        ZoneScoped;
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx, "MouseScrollCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        ctx->glfwEventQueue.Add(ecs::Event{INPUT_EVENT_MOUSE_SCROLL, mouse, glm::vec2(xOffset, yOffset)});
    }
} // namespace sp
