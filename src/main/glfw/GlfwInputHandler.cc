/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "GlfwInputHandler.hh"

#include "GlfwKeyCodes.hh"
#include "glm/gtx/string_cast.hpp"
#include "input.h"
#include "strayphotons/HeapString.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/input/BindingNames.hh"
#include "strayphotons/input/KeyCodes.hh"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <strayphotons.h>
#include <string>
#include <tracy/Tracy.hpp>

namespace sp {
    GlfwInputHandler::GlfwInputHandler(sp_game_t *ctx, GLFWwindow *window) : ctx(ctx), window(window) {
        if (window) {
            glfwSetWindowUserPointer(window, this);

            glfwSetKeyCallback(window, KeyInputCallback);
            glfwSetCharCallback(window, CharInputCallback);
            glfwSetScrollCallback(window, MouseScrollCallback);
            glfwSetMouseButtonCallback(window, MouseButtonCallback);
            glfwSetCursorPosCallback(window, MouseMoveCallback);
            glfwSetCursorEnterCallback(window, MouseEnterCallback);

            for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; jid++) {
                if (glfwJoystickPresent(jid)) {
                    HeapString joystickName = "joystick" + std::to_string(jid);
                    joysticks[jid] = sp_new_input_device(ctx, joystickName.c_str());
                    int gamepad = glfwJoystickIsGamepad(jid);
                    Logf("Joystick %d: gamepad %d", jid, gamepad);
                    sp_send_input_bool(ctx, joysticks[jid], "/joystick/present", true);
                }
            }
            static GlfwInputHandler *handler = this;
            auto JoystickCallback = [](int jid, int event) {
                if (event == GLFW_CONNECTED) {
                    if (!handler->joysticks[jid]) {
                        Logf("New controller detected: %d", jid);
                        HeapString joystickName = "joystick" + std::to_string(jid);
                        handler->joysticks[jid] = sp_new_input_device(handler->ctx, joystickName.c_str());
                    }
                    Logf("Controller connected: %d", jid);
                    sp_send_input_bool(handler->ctx, handler->joysticks[jid], "/joystick/present", true);
                } else if (event == GLFW_DISCONNECTED) {
                    Logf("Controller disconnected: %d", jid);
                    sp_send_input_bool(handler->ctx, handler->joysticks[jid], "/joystick/present", false);
                } else {
                    Errorf("Unknown glfw joystick callback: jid %d event %d", jid, event);
                }
            };
            glfwSetJoystickCallback(JoystickCallback);
        }

        mouse = sp_new_input_device(ctx, "mouse");
        keyboard = sp_new_input_device(ctx, "keyboard");
    }

    GlfwInputHandler::~GlfwInputHandler() {
        if (window) {
            glfwSetKeyCallback(window, nullptr);
            glfwSetCharCallback(window, nullptr);
            glfwSetScrollCallback(window, nullptr);
            glfwSetMouseButtonCallback(window, nullptr);
            glfwSetCursorPosCallback(window, nullptr);
            glfwSetCursorEnterCallback(window, nullptr);

            glfwSetWindowUserPointer(window, nullptr);
        }
    }

    void GlfwInputHandler::Frame() {
        ZoneScoped;
        glfwPollEvents();
        for (int jid = GLFW_JOYSTICK_1; jid < joysticks.size(); jid++) {
            if (!joysticks[jid]) continue;
            int axisCount = 0;
            const float *axes = glfwGetJoystickAxes(jid, &axisCount);
            for (int axis = 0; axis < axisCount; axis++) {
                HeapString axisName = "/joystick/axis" + std::to_string(axis);
                sp_send_input_float(ctx, joysticks[jid], axisName.c_str(), axes[axis]);
            }
            int buttonCount = 0;
            const uint8_t *buttons = glfwGetJoystickButtons(jid, &buttonCount);
            for (int button = 0; button < buttonCount; button++) {
                HeapString buttonName = "/joystick/button" + std::to_string(button);
                sp_send_input_bool(ctx, joysticks[jid], buttonName.c_str(), buttons[button] == GLFW_PRESS);
            }
        }
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
            sp_send_input_int(handler->ctx, handler->keyboard, INPUT_EVENT_KEYBOARD_KEY_DOWN.c_str(), keyCode->second);
        } else if (action == GLFW_RELEASE) {
            sp_send_input_int(handler->ctx, handler->keyboard, INPUT_EVENT_KEYBOARD_KEY_UP.c_str(), keyCode->second);
        }
    }

    void GlfwInputHandler::CharInputCallback(GLFWwindow *window, unsigned int codepoint) {
        ZoneScoped;
        auto handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "CharInputCallback occured without valid context");

        sp_send_input_uint(handler->ctx, handler->keyboard, INPUT_EVENT_KEYBOARD_CHARACTERS.c_str(), codepoint);
    }

    void GlfwInputHandler::MouseMoveCallback(GLFWwindow *window, double xPos, double yPos) {
        ZoneScoped;
        GlfwInputHandler *handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "MouseMoveCallback occured without valid context");

        glm::vec2 windowScale(1.0f);
#ifndef _WIN32
        glfwGetWindowContentScale(window, &windowScale.x, &windowScale.y);
#endif
        sp_send_input_vec2(handler->ctx,
            handler->mouse,
            INPUT_EVENT_MOUSE_POSITION.c_str(),
            xPos * windowScale.x,
            yPos * windowScale.y);

        int mouseMode = glfwGetInputMode(window, GLFW_CURSOR);
        if (!glm::any(glm::isinf(handler->prevMousePos)) && handler->prevMouseMode == mouseMode) {
            sp_send_input_vec2(handler->ctx,
                handler->mouse,
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
            sp_send_input_bool(handler->ctx,
                handler->mouse,
                INPUT_EVENT_MOUSE_LEFT_CLICK.c_str(),
                action == GLFW_PRESS);
        } else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            sp_send_input_bool(handler->ctx,
                handler->mouse,
                INPUT_EVENT_MOUSE_MIDDLE_CLICK.c_str(),
                action == GLFW_PRESS);
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            sp_send_input_bool(handler->ctx,
                handler->mouse,
                INPUT_EVENT_MOUSE_RIGHT_CLICK.c_str(),
                action == GLFW_PRESS);
        }
    }

    void GlfwInputHandler::MouseScrollCallback(GLFWwindow *window, double xOffset, double yOffset) {
        ZoneScoped;
        auto handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "MouseScrollCallback occured without valid context");

        sp_send_input_vec2(handler->ctx, handler->mouse, INPUT_EVENT_MOUSE_SCROLL.c_str(), xOffset, yOffset);
    }

    void GlfwInputHandler::MouseEnterCallback(GLFWwindow *window, int entered) {
        ZoneScoped;
        auto handler = static_cast<GlfwInputHandler *>(glfwGetWindowUserPointer(window));
        Assert(handler, "MouseEnterCallback occured without valid context");

        if (entered) {
            glm::dvec2 dpos = glm::dvec2(0.0);
            glfwGetCursorPos(window, &dpos.x, &dpos.y);
            handler->prevMousePos = dpos;
        } else {
            handler->prevMousePos = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
        }
    }
} // namespace sp
