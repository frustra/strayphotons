#pragma once

#include <robin_hood.h>
#include <string>

namespace sp {
    // Global Mouse + Keyboard
    static const std::string INPUT_EVENT_KEYBOARD_KEY_BASE = "/keyboard/key/"; // bool
    static const std::string INPUT_EVENT_KEYBOARD_CHARACTERS = "/keyboard/characters"; // char
    static const std::string INPUT_EVENT_MOUSE_CLICK = "/mouse/click"; // glm::vec2
    static const std::string INPUT_EVENT_MOUSE_SCROLL = "/mouse/scroll"; // glm::vec2
    static const std::string INPUT_EVENT_MOUSE_MOVE = "/mouse/move"; // glm::vec2

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
    static const std::string INPUT_EVENT_MENU_ENTER = "/action/menu/enter"; // bool
    static const std::string INPUT_EVENT_MENU_SCROLL = "/action/menu/scroll"; // glm::vec2
    static const std::string INPUT_EVENT_MENU_TEXT_INPUT = "/action/menu/text_input"; // char

    static const std::string INPUT_SIGNAL_MENU_PRIMARY_TRIGGER = "menu_primary_trigger";
    static const std::string INPUT_SIGNAL_MENU_SECONDARY_TRIGGER = "menu_secondary_trigger";
    static const std::string INPUT_SIGNAL_MENU_CURSOR_X = "menu_cursor_x";
    static const std::string INPUT_SIGNAL_MENU_CURSOR_Y = "menu_cursor_y";

    // CharacterController
    static const std::string INPUT_SIGNAL_MOVE_WORLD_X = "move_world_x";
    static const std::string INPUT_SIGNAL_MOVE_WORLD_Y = "move_world_y";
    static const std::string INPUT_SIGNAL_MOVE_WORLD_Z = "move_world_z";

    // Player / HumanController
    static const std::string INPUT_SIGNAL_MOVE_FORWARD = "move_forward";
    static const std::string INPUT_SIGNAL_MOVE_LEFT = "move_left";
    static const std::string INPUT_SIGNAL_MOVE_RIGHT = "move_right";
    static const std::string INPUT_SIGNAL_MOVE_BACK = "move_back";
    static const std::string INPUT_SIGNAL_MOVE_CROUCH = "move_crouch";
    static const std::string INPUT_SIGNAL_MOVE_JUMP = "move_jump";
    static const std::string INPUT_SIGNAL_MOVE_SPRINT = "move_sprint";
    static const std::string INPUT_SIGNAL_INTERACT_ROTATE = "interact_rotate";

    static const std::string INPUT_EVENT_CAMERA_ROTATE = "/action/camera_rotate"; // glm::vec2
    static const std::string INPUT_EVENT_INTERACT = "/action/interact"; // bool
    static const std::string INPUT_EVENT_TELEPORT = "/action/teleport"; // bool

    // Other
    static const std::string INPUT_EVENT_RUN_COMMAND = "/action/run_command"; // string
    static const std::string INPUT_EVENT_SPAWN_DEBUG = "/action/spawn_debug"; // bool
} // namespace sp
