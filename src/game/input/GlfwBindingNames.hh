#pragma once

#include <GLFW/glfw3.h>
#include <robin_hood.h>
#include <string>
#include "InputManager.hh"

namespace sp
{
    static const robin_hood::unordered_flat_map<int, std::string> GlfwKeyActionNames =
    {
        {GLFW_KEY_SPACE, "space"},
        {GLFW_KEY_APOSTROPHE, "apostrophe"},
        {GLFW_KEY_COMMA, "comma"},
        {GLFW_KEY_MINUS, "minus"},
        {GLFW_KEY_PERIOD, "period"},
        {GLFW_KEY_SLASH, "slash"},
        {GLFW_KEY_0, "0"},
        {GLFW_KEY_1, "1"},
        {GLFW_KEY_2, "2"},
        {GLFW_KEY_3, "3"},
        {GLFW_KEY_4, "4"},
        {GLFW_KEY_5, "5"},
        {GLFW_KEY_6, "6"},
        {GLFW_KEY_7, "7"},
        {GLFW_KEY_8, "8"},
        {GLFW_KEY_9, "9"},
        {GLFW_KEY_SEMICOLON, "semicolon"},
        {GLFW_KEY_EQUAL, "equals"},
        {GLFW_KEY_A, "a"},
        {GLFW_KEY_B, "b"},
        {GLFW_KEY_C, "c"},
        {GLFW_KEY_D, "d"},
        {GLFW_KEY_E, "e"},
        {GLFW_KEY_F, "f"},
        {GLFW_KEY_G, "g"},
        {GLFW_KEY_H, "h"},
        {GLFW_KEY_I, "i"},
        {GLFW_KEY_J, "j"},
        {GLFW_KEY_K, "k"},
        {GLFW_KEY_L, "l"},
        {GLFW_KEY_M, "m"},
        {GLFW_KEY_N, "n"},
        {GLFW_KEY_O, "o"},
        {GLFW_KEY_P, "p"},
        {GLFW_KEY_Q, "q"},
        {GLFW_KEY_R, "r"},
        {GLFW_KEY_S, "s"},
        {GLFW_KEY_T, "t"},
        {GLFW_KEY_U, "u"},
        {GLFW_KEY_V, "v"},
        {GLFW_KEY_W, "w"},
        {GLFW_KEY_X, "x"},
        {GLFW_KEY_Y, "y"},
        {GLFW_KEY_Z, "z"},
        {GLFW_KEY_LEFT_BRACKET, "left-bracket"},
        {GLFW_KEY_BACKSLASH, "backslash"},
        {GLFW_KEY_RIGHT_BRACKET, "right-bracket"},
        {GLFW_KEY_GRAVE_ACCENT, "backtick"},
        {GLFW_KEY_ESCAPE, "escape"},
        {GLFW_KEY_ENTER, "enter"},
        {GLFW_KEY_TAB, "tag"},
        {GLFW_KEY_BACKSPACE, "backspace"},
        {GLFW_KEY_INSERT, "insert"},
        {GLFW_KEY_DELETE, "delete"},
        {GLFW_KEY_RIGHT, "arrow_right"},
        {GLFW_KEY_LEFT, "arrow_left"},
        {GLFW_KEY_DOWN, "arrow_down"},
        {GLFW_KEY_UP, "arrow_up"},
        {GLFW_KEY_PAGE_UP, "page-up"},
        {GLFW_KEY_PAGE_DOWN, "page-down"},
        {GLFW_KEY_HOME, "home"},
        {GLFW_KEY_END, "end"},
        {GLFW_KEY_CAPS_LOCK, "caps-lock"},
        {GLFW_KEY_SCROLL_LOCK, "scroll-lock"},
        {GLFW_KEY_NUM_LOCK, "num-lock"},
        {GLFW_KEY_PRINT_SCREEN, "print-screen"},
        {GLFW_KEY_PAUSE, "pause"},
        {GLFW_KEY_F1, "f1"},
        {GLFW_KEY_F2, "f2"},
        {GLFW_KEY_F3, "f3"},
        {GLFW_KEY_F4, "f4"},
        {GLFW_KEY_F5, "f5"},
        {GLFW_KEY_F6, "f6"},
        {GLFW_KEY_F7, "f7"},
        {GLFW_KEY_F8, "f8"},
        {GLFW_KEY_F9, "f9"},
        {GLFW_KEY_F10, "f10"},
        {GLFW_KEY_F11, "f11"},
        {GLFW_KEY_F12, "f12"},
        {GLFW_KEY_F13, "f13"},
        {GLFW_KEY_F14, "f14"},
        {GLFW_KEY_F15, "f15"},
        {GLFW_KEY_F16, "f16"},
        {GLFW_KEY_F17, "f17"},
        {GLFW_KEY_F18, "f18"},
        {GLFW_KEY_F19, "f19"},
        {GLFW_KEY_F20, "f20"},
        {GLFW_KEY_F21, "f21"},
        {GLFW_KEY_F22, "f22"},
        {GLFW_KEY_F23, "f23"},
        {GLFW_KEY_F24, "f24"},
        {GLFW_KEY_F25, "f25"},
        {GLFW_KEY_KP_0, "0_numpad"},
        {GLFW_KEY_KP_1, "1_numpad"},
        {GLFW_KEY_KP_2, "2_numpad"},
        {GLFW_KEY_KP_3, "3_numpad"},
        {GLFW_KEY_KP_4, "4_numpad"},
        {GLFW_KEY_KP_5, "5_numpad"},
        {GLFW_KEY_KP_6, "6_numpad"},
        {GLFW_KEY_KP_7, "7_numpad"},
        {GLFW_KEY_KP_8, "8_numpad"},
        {GLFW_KEY_KP_9, "9_numpad"},
        {GLFW_KEY_KP_DECIMAL, "period_numpad"},
        {GLFW_KEY_KP_DIVIDE, "divide_numpad"},
        {GLFW_KEY_KP_MULTIPLY, "multiply_numpad"},
        {GLFW_KEY_KP_SUBTRACT, "minus_numpad"},
        {GLFW_KEY_KP_ADD, "plus_numpad"},
        {GLFW_KEY_KP_ENTER, "enter_numpad"},
        {GLFW_KEY_KP_EQUAL, "equals_numpad"},
        {GLFW_KEY_LEFT_SHIFT, "shift_left"},
        {GLFW_KEY_LEFT_CONTROL, "control_left"},
        {GLFW_KEY_LEFT_ALT, "alt_left"},
        {GLFW_KEY_LEFT_SUPER, "super_left"},
        {GLFW_KEY_RIGHT_SHIFT, "shift_right"},
        {GLFW_KEY_RIGHT_CONTROL, "control_right"},
        {GLFW_KEY_RIGHT_ALT, "alt_right"},
        {GLFW_KEY_RIGHT_SUPER, "super_right"},
    };

    static bool ActionPathFromGlfwKey(int key, std::string &actionPath)
    {
        auto it = GlfwKeyActionNames.find(key);
        if (it != GlfwKeyActionNames.end())
        {
            actionPath = INPUT_ACTION_KEYBOARD_BASE + "/" + it->second;
            return true;
        }
        else
        {
            return false;
        }
    }
    
    static const robin_hood::unordered_flat_map<int, std::string> GlfwMouseButtonActionNames =
    {
        {GLFW_MOUSE_BUTTON_LEFT, "button_left"},
        {GLFW_MOUSE_BUTTON_MIDDLE, "button_middle"},
        {GLFW_MOUSE_BUTTON_RIGHT, "button_right"},
    };

    static bool ActionPathFromGlfwMouseButton(int button, std::string &actionPath)
    {
        auto it = GlfwMouseButtonActionNames.find(button);
        if (it != GlfwMouseButtonActionNames.end())
        {
            actionPath = INPUT_ACTION_MOUSE_BASE + "/" + it->second;
            return true;
        }
        else
        {
            return false;
        }
    }
}
