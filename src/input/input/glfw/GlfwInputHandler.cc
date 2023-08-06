/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GlfwInputHandler.hh"

#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
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
    GlfwInputHandler::GlfwInputHandler(LockFreeEventQueue<ecs::Event> &windowEventQueue, GLFWwindow &glfwWindow)
        : outputEventQueue(windowEventQueue), window(&glfwWindow) {
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

                auto mouse = scene->NewSystemEntity(lock, scene, mouseEntity.Name());
                mouse.Set<ecs::EventBindings>(lock);
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
        glfwPollEvents();
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
            ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_KEYBOARD_KEY_DOWN, keyboard, keyName});
        } else if (action == GLFW_RELEASE) {
            ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_KEYBOARD_KEY_UP, keyboard, keyName});
        }
    }

    void GlfwInputHandler::CharInputCallback(GLFWwindow *window, unsigned int ch) {
        ZoneScoped;
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx, "CharInputCallback occured without valid context");

        auto keyboard = ctx->keyboardEntity.GetLive();
        // TODO: Handle unicode somehow?
        ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_KEYBOARD_CHARACTERS, keyboard, (char)ch});
    }

    void GlfwInputHandler::MouseMoveCallback(GLFWwindow *window, double xPos, double yPos) {
        ZoneScoped;
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx, "MouseMoveCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_MOUSE_POSITION, mouse, glm::vec2(xPos, yPos)});
    }

    void GlfwInputHandler::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        ZoneScoped;
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx, "MouseButtonCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_MOUSE_LEFT_CLICK, mouse, action == GLFW_PRESS});
        } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_MOUSE_MIDDLE_CLICK, mouse, action == GLFW_PRESS});
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_MOUSE_RIGHT_CLICK, mouse, action == GLFW_PRESS});
        }
    }

    void GlfwInputHandler::MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
        ZoneScoped;
        auto ctx = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(ctx, "MouseScrollCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_MOUSE_SCROLL, mouse, glm::vec2(xOffset, yOffset)});
    }
} // namespace sp
