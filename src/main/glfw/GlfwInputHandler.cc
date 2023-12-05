/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GlfwInputHandler.hh"

#include "GlfwKeyCodes.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/CGameContext.hh"
#include "game/Game.hh"
#include "game/Scene.hh"
#include "game/SceneManager.hh"
#include "graphics/GraphicsManager.hh"
#include "graphics/core/GraphicsContext.hh"
#include "input/BindingNames.hh"
#include "input/KeyCodes.hh"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/glm.hpp>
#include <stdexcept>
#include <strayphotons.h>

namespace sp {
    GlfwInputHandler::GlfwInputHandler(CGameContext *ctx) : ctx(ctx), outputEventQueue(ctx->game.inputEventQueue) {
        GraphicsContext *gfxContext = ctx->game.graphics->context.get();
        if (gfxContext) window = gfxContext->GetGlfwWindow();

        if (window) {
            glfwSetWindowUserPointer(window, this);

            glfwSetKeyCallback(window, KeyInputCallback);
            glfwSetCharCallback(window, CharInputCallback);
            glfwSetScrollCallback(window, MouseScrollCallback);
            glfwSetMouseButtonCallback(window, MouseButtonCallback);
            glfwSetCursorPosCallback(window, MouseMoveCallback);
            glfwSetCursorEnterCallback(window, MouseEnterCallback);
        }

        mouse = sp::game_new_input_device(ctx, "mouse");
        keyboard = sp::game_new_input_device(ctx, "keyboard");
    }

    GlfwInputHandler::~GlfwInputHandler() {
        if (window) {
            glfwSetKeyCallback(window, nullptr);
            glfwSetCharCallback(window, nullptr);
            glfwSetScrollCallback(window, nullptr);
            glfwSetMouseButtonCallback(window, nullptr);
            glfwSetCursorPosCallback(window, nullptr);

            glfwSetWindowUserPointer(window, nullptr);
        }
    }

    void GlfwInputHandler::Frame() {
        ZoneScoped;
        glfwPollEvents();
    }

    void GlfwInputHandler::KeyInputCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
        ZoneScoped;
        if (key == GLFW_KEY_UNKNOWN) return;

        auto handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "KeyInputCallback occured without valid context");

        // TODO: Possibly lookup based on glfwGetKeyName() crossreference?
        auto keyCode = GlfwKeyMapping.find(key);
        if (keyCode == GlfwKeyMapping.end()) {
            Errorf("Unknown glfw keycode: %d", key);
            return;
        }

        if (action == GLFW_PRESS) {
            sp::game_send_input_int(handler->ctx,
                (uint64_t)handler->keyboard,
                INPUT_EVENT_KEYBOARD_KEY_DOWN.c_str(),
                keyCode->second);
        } else if (action == GLFW_RELEASE) {
            sp::game_send_input_int(handler->ctx,
                (uint64_t)handler->keyboard,
                INPUT_EVENT_KEYBOARD_KEY_UP.c_str(),
                keyCode->second);
        }
    }

    void GlfwInputHandler::CharInputCallback(GLFWwindow *window, unsigned int codepoint) {
        ZoneScoped;
        auto handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "CharInputCallback occured without valid context");

        sp::game_send_input_uint(handler->ctx,
            (uint64_t)handler->keyboard,
            INPUT_EVENT_KEYBOARD_CHARACTERS.c_str(),
            codepoint);
    }

    void GlfwInputHandler::MouseMoveCallback(GLFWwindow *window, double xPos, double yPos) {
        ZoneScoped;
        auto handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "MouseMoveCallback occured without valid context");

        sp::game_send_input_vec2(handler->ctx,
            (uint64_t)handler->mouse,
            INPUT_EVENT_MOUSE_POSITION.c_str(),
            xPos,
            yPos);

        int mouseMode = glfwGetInputMode(window, GLFW_CURSOR);
        if (!glm::any(glm::isinf(handler->prevMousePos)) && handler->prevMouseMode == mouseMode) {
            sp::game_send_input_vec2(handler->ctx,
                (uint64_t)handler->mouse,
                INPUT_EVENT_MOUSE_MOVE.c_str(),
                xPos - handler->prevMousePos.x,
                yPos - handler->prevMousePos.y);
        }
        handler->prevMousePos = glm::vec2(xPos, yPos);
        handler->prevMouseMode = mouseMode;
    }

    void GlfwInputHandler::MouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
        ZoneScoped;
        auto handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "MouseButtonCallback occured without valid context");

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            sp::game_send_input_bool(handler->ctx,
                (uint64_t)handler->mouse,
                INPUT_EVENT_MOUSE_LEFT_CLICK.c_str(),
                action == GLFW_PRESS);
        } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            sp::game_send_input_bool(handler->ctx,
                (uint64_t)handler->mouse,
                INPUT_EVENT_MOUSE_MIDDLE_CLICK.c_str(),
                action == GLFW_PRESS);
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            sp::game_send_input_bool(handler->ctx,
                (uint64_t)handler->mouse,
                INPUT_EVENT_MOUSE_RIGHT_CLICK.c_str(),
                action == GLFW_PRESS);
        }
    }

    void GlfwInputHandler::MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
        ZoneScoped;
        auto handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "MouseScrollCallback occured without valid context");

        sp::game_send_input_vec2(handler->ctx,
            (uint64_t)handler->mouse,
            INPUT_EVENT_MOUSE_SCROLL.c_str(),
            xOffset,
            yOffset);
    }

    void GlfwInputHandler::MouseEnterCallback(GLFWwindow *window, int entered) {
        ZoneScoped;
        auto handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "MouseEnterCallback occured without valid context");

        if (entered) {
            glm::dvec2 dpos;
            glfwGetCursorPos(window, &dpos.x, &dpos.y);
            handler->prevMousePos = dpos;
        } else {
            handler->prevMousePos = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
        }
    }
} // namespace sp
