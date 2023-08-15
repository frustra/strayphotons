/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "WinitInputHandler.hh"

#include "GraphicsManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "graphics/core/GraphicsContext.hh"
#include "input/BindingNames.hh"

#include <algorithm>
#include <glm/glm.hpp>
#include <stdexcept>
#include <winit.rs.h>

namespace sp {
    WinitInputHandler::WinitInputHandler(GraphicsManager &manager,
        LockFreeEventQueue<ecs::Event> &windowEventQueue,
        winit::WinitContext &context)
        : manager(manager), outputEventQueue(windowEventQueue), context(context) {
        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "input",
            [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto keyboard = scene->NewSystemEntity(lock, scene, keyboardEntity.Name());
                keyboard.Set<ecs::EventBindings>(lock);

                auto mouse = scene->NewSystemEntity(lock, scene, mouseEntity.Name());
                mouse.Set<ecs::EventBindings>(lock);
            });
    }

    void WinitInputHandler::StartEventLoop(uint32_t maxInputRate) {
        start_event_loop(context, this, maxInputRate);
    }

    bool InputFrameCallback(WinitInputHandler *ctx) {
        ZoneScoped;
        Assert(ctx, "KeyInputCallback occured without valid context");

        return ctx->manager.InputFrame();
    }

    void KeyInputCallback(WinitInputHandler *ctx, KeyCode key, int scancode, InputAction action) {
        ZoneScoped;
        Assert(ctx, "KeyInputCallback occured without valid context");
        if (key == KeyCode::KEY_INVALID) return;

        auto keyName = KeycodeNameLookup.at(key);

        auto keyboard = ctx->keyboardEntity.GetLive();
        std::string eventName = INPUT_EVENT_KEYBOARD_KEY_BASE + keyName;
        if (action == InputAction::PRESS) {
            ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_KEYBOARD_KEY_DOWN, keyboard, keyName});
        } else if (action == InputAction::RELEASE) {
            ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_KEYBOARD_KEY_UP, keyboard, keyName});
        }
    }

    void CharInputCallback(WinitInputHandler *ctx, unsigned int ch) {
        ZoneScoped;
        Assert(ctx, "CharInputCallback occured without valid context");

        auto keyboard = ctx->keyboardEntity.GetLive();
        // TODO: Handle unicode somehow?
        ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_KEYBOARD_CHARACTERS, keyboard, (char)ch});
    }

    void MouseMoveCallback(WinitInputHandler *ctx, double dx, double dy) {
        ZoneScoped;
        Assert(ctx, "MouseMoveCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_MOUSE_MOVE, mouse, glm::vec2(dx, dy)});
    }

    void MousePositionCallback(WinitInputHandler *ctx, double xPos, double yPos) {
        ZoneScoped;
        Assert(ctx, "MousePositionCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_MOUSE_POSITION, mouse, glm::vec2(xPos, yPos)});
    }

    void MouseButtonCallback(WinitInputHandler *ctx, MouseButton button, InputAction action) {
        ZoneScoped;
        Assert(ctx, "MouseButtonCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        if (button == MouseButton::BUTTON_LEFT) {
            ctx->outputEventQueue.PushEvent(
                ecs::Event{INPUT_EVENT_MOUSE_LEFT_CLICK, mouse, action == InputAction::PRESS});
        } else if (button == MouseButton::BUTTON_MIDDLE) {
            ctx->outputEventQueue.PushEvent(
                ecs::Event{INPUT_EVENT_MOUSE_MIDDLE_CLICK, mouse, action == InputAction::PRESS});
        } else if (button == MouseButton::BUTTON_RIGHT) {
            ctx->outputEventQueue.PushEvent(
                ecs::Event{INPUT_EVENT_MOUSE_RIGHT_CLICK, mouse, action == InputAction::PRESS});
        }
    }

    void MouseScrollCallback(WinitInputHandler *ctx, double xOffset, double yOffset) {
        ZoneScoped;
        Assert(ctx, "MouseScrollCallback occured without valid context");

        auto mouse = ctx->mouseEntity.GetLive();
        ctx->outputEventQueue.PushEvent(ecs::Event{INPUT_EVENT_MOUSE_SCROLL, mouse, glm::vec2(xOffset, yOffset)});
    }
} // namespace sp
