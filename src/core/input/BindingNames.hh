/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <robin_hood.h>
#include <string>

namespace sp {
    // Global Mouse + Keyboard
    static const std::string INPUT_EVENT_KEYBOARD_KEY_BASE = "/keyboard/key/"; // bool
    static const std::string INPUT_EVENT_KEYBOARD_KEY_DOWN = "/keyboard/key_down"; // string
    static const std::string INPUT_EVENT_KEYBOARD_KEY_UP = "/keyboard/key_up"; // string
    static const std::string INPUT_EVENT_KEYBOARD_CHARACTERS = "/keyboard/characters"; // char
    static const std::string INPUT_EVENT_MOUSE_LEFT_CLICK = "/mouse/left_click"; // bool
    static const std::string INPUT_EVENT_MOUSE_MIDDLE_CLICK = "/mouse/middle_click"; // bool
    static const std::string INPUT_EVENT_MOUSE_RIGHT_CLICK = "/mouse/right_click"; // bool
    static const std::string INPUT_EVENT_MOUSE_SCROLL = "/mouse/scroll"; // glm::vec2
    static const std::string INPUT_EVENT_MOUSE_MOVE = "/mouse/move"; // glm::vec2
    static const std::string INPUT_EVENT_MOUSE_POSITION = "/mouse/pos"; // glm::vec2

    static const std::string INPUT_SIGNAL_KEYBOARD_KEY_BASE = "key_";
    static const std::string INPUT_SIGNAL_MOUSE_BUTTON_LEFT = "mouse_button_left";
    static const std::string INPUT_SIGNAL_MOUSE_BUTTON_MIDDLE = "mouse_button_middle";
    static const std::string INPUT_SIGNAL_MOUSE_BUTTON_RIGHT = "mouse_button_right";
    static const std::string INPUT_SIGNAL_MOUSE_CURSOR_X = "mouse_cursor_x";
    static const std::string INPUT_SIGNAL_MOUSE_CURSOR_Y = "mouse_cursor_y";

    // Menu
    static const std::string INPUT_EVENT_TOGGLE_CONSOLE = "/action/console/toggle"; // bool
    static const std::string INPUT_EVENT_MENU_OPEN = "/action/menu/open"; // bool
    static const std::string INPUT_EVENT_MENU_BACK = "/action/menu/back"; // bool
    static const std::string INPUT_EVENT_MENU_SCROLL = "/action/menu/scroll"; // glm::vec2
    static const std::string INPUT_EVENT_MENU_CURSOR = "/action/menu/cursor"; // glm::vec2
    static const std::string INPUT_EVENT_MENU_PRIMARY_TRIGGER = "/action/menu/primary_trigger"; // bool
    static const std::string INPUT_EVENT_MENU_SECONDARY_TRIGGER = "/action/menu/secondary_trigger"; // bool
    static const std::string INPUT_EVENT_MENU_TEXT_INPUT = "/action/menu/text_input"; // char
    static const std::string INPUT_EVENT_MENU_KEY_DOWN = "/action/menu/key_down"; // string
    static const std::string INPUT_EVENT_MENU_KEY_UP = "/action/menu/key_up"; // string

    // CharacterController
    static const std::string INPUT_SIGNAL_MOVE_RELATIVE_X = "move_relative.x";
    static const std::string INPUT_SIGNAL_MOVE_RELATIVE_Y = "move_relative.y";
    static const std::string INPUT_SIGNAL_MOVE_RELATIVE_Z = "move_relative.z";
    static const std::string INPUT_SIGNAL_MOVE_SPRINT = "move_sprint";
    static const std::string INPUT_SIGNAL_MOVE_NOCLIP = "move_noclip";

    // Interaction
    static const std::string INTERACT_EVENT_INTERACT_POINT = "/interact/point"; // ecs::Transform, vec2 or false
    static const std::string INTERACT_EVENT_INTERACT_PRESS = "/interact/press"; // bool
    static const std::string INTERACT_EVENT_INTERACT_GRAB = "/interact/grab"; // ecs::Transform or false
    static const std::string INTERACT_EVENT_INTERACT_ROTATE = "/interact/rotate"; // glm::vec2

    // Physics
    static const std::string PHYSICS_EVENT_COLLISION_FORCE_FOUND = "/physics/collision/force_found"; // float
    static const std::string PHYSICS_EVENT_COLLISION_FORCE_LOST = "/physics/collision/force_lost"; // float

    // Editor Control
    static const std::string EDITOR_EVENT_EDIT_TARGET = "/edit/target"; // ecs::Entity

    // Other
    static const std::string ACTION_EVENT_RUN_COMMAND = "/action/run_command"; // string
} // namespace sp
