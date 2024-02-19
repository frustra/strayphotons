/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifdef SP_RUST_WINIT_SUPPORT
    #include "WinitInputHandler.hh"

    #include "common/Common.hh"
    #include "common/Logging.hh"
    #include "common/Tracing.hh"
    #include "ecs/EcsImpl.hh"
    #include "game/CGameContext.hh"
    #include "game/Game.hh"
    #include "game/Scene.hh"
    #include "game/SceneManager.hh"
    #include "graphics/GraphicsManager.hh"
    #include "graphics/core/GraphicsContext.hh"
    #include "input/BindingNames.hh"

    #include <algorithm>
    #include <glm/glm.hpp>
    #include <stdexcept>
    #include <strayphotons.h>
    #include <winit.rs.h>

namespace sp {
    WinitInputHandler::WinitInputHandler(CGameContext *ctx) : ctx(ctx) {
        GraphicsContext *gfxContext = ctx->game.graphics->context.get();
        if (gfxContext) context = gfxContext->GetWinitContext();

        mouse = sp_new_input_device(ctx, "mouse");
        keyboard = sp_new_input_device(ctx, "keyboard");
    }

    void WinitInputHandler::StartEventLoop(uint32_t maxInputRate) {
        start_event_loop(*context, this, maxInputRate);
    }

    bool InputFrameCallback(WinitInputHandler *handler) {
        ZoneScoped;
        Assert(handler, "KeyInputCallback occured without valid context");

        return handler->ctx->game.graphics->InputFrame() && !handler->ctx->game.exitTriggered.test();
    }

    void KeyInputCallback(WinitInputHandler *handler, KeyCode key, int scancode, InputAction action) {
        ZoneScoped;
        Assert(handler, "KeyInputCallback occured without valid context");
        if (key == KeyCode::KEY_INVALID) return;

        if (action == InputAction::PRESS) {
            sp_send_input_int(handler->ctx, (uint64_t)handler->keyboard, INPUT_EVENT_KEYBOARD_KEY_DOWN.c_str(), key);
        } else if (action == InputAction::RELEASE) {
            sp_send_input_int(handler->ctx, (uint64_t)handler->keyboard, INPUT_EVENT_KEYBOARD_KEY_UP.c_str(), key);
        }
    }

    void CharInputCallback(WinitInputHandler *handler, unsigned int codepoint) {
        ZoneScoped;
        Assert(handler, "CharInputCallback occured without valid context");

        sp_send_input_uint(handler->ctx,
            (uint64_t)handler->keyboard,
            INPUT_EVENT_KEYBOARD_CHARACTERS.c_str(),
            codepoint);
    }

    void MouseMoveCallback(WinitInputHandler *handler, double dx, double dy) {
        ZoneScoped;
        Assert(handler, "MouseMoveCallback occured without valid context");

        sp_send_input_vec2(handler->ctx, (uint64_t)handler->mouse, INPUT_EVENT_MOUSE_MOVE.c_str(), dx, dy);
    }

    void MousePositionCallback(WinitInputHandler *handler, double xPos, double yPos) {
        ZoneScoped;
        Assert(handler, "MousePositionCallback occured without valid context");

        sp_send_input_vec2(handler->ctx, (uint64_t)handler->mouse, INPUT_EVENT_MOUSE_POSITION.c_str(), xPos, yPos);
    }

    void MouseButtonCallback(WinitInputHandler *handler, MouseButton button, InputAction action) {
        ZoneScoped;
        Assert(handler, "MouseButtonCallback occured without valid context");

        if (button == MouseButton::BUTTON_LEFT) {
            sp_send_input_bool(handler->ctx,
                (uint64_t)handler->mouse,
                INPUT_EVENT_MOUSE_LEFT_CLICK.c_str(),
                action == InputAction::PRESS);
        } else if (button == MouseButton::BUTTON_MIDDLE) {
            sp_send_input_bool(handler->ctx,
                (uint64_t)handler->mouse,
                INPUT_EVENT_MOUSE_MIDDLE_CLICK.c_str(),
                action == InputAction::PRESS);
        } else if (button == MouseButton::BUTTON_RIGHT) {
            sp_send_input_bool(handler->ctx,
                (uint64_t)handler->mouse,
                INPUT_EVENT_MOUSE_RIGHT_CLICK.c_str(),
                action == InputAction::PRESS);
        }
    }

    void MouseScrollCallback(WinitInputHandler *handler, double xOffset, double yOffset) {
        ZoneScoped;
        Assert(handler, "MouseScrollCallback occured without valid context");

        sp_send_input_vec2(handler->ctx, (uint64_t)handler->mouse, INPUT_EVENT_MOUSE_SCROLL.c_str(), xOffset, yOffset);
    }
} // namespace sp

#endif
