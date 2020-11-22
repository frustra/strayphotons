#pragma once

#include <robin_hood.h>
#include <string>

namespace sp {
    // All inputs in UPPERCASE to allow for case-insensitivity
    static robin_hood::unordered_flat_map<std::string, std::string> UserBindingNames = {
        {"0", "0"},
        {"1", "1"},
        {"2", "2"},
        {"3", "3"},
        {"4", "4"},
        {"5", "5"},
        {"6", "6"},
        {"7", "7"},
        {"8", "8"},
        {"9", "9"},
        {"A", "a"},
        {"B", "b"},
        {"C", "c"},
        {"D", "d"},
        {"E", "e"},
        {"F", "f"},
        {"G", "g"},
        {"H", "h"},
        {"I", "i"},
        {"J", "j"},
        {"K", "k"},
        {"L", "l"},
        {"M", "m"},
        {"N", "n"},
        {"O", "o"},
        {"P", "p"},
        {"Q", "q"},
        {"R", "r"},
        {"S", "s"},
        {"T", "t"},
        {"U", "u"},
        {"V", "v"},
        {"W", "w"},
        {"X", "x"},
        {"Y", "y"},
        {"Z", "z"},

        {"SPACE", "space"},
        {" ", "space"},
        {"APOSTROPHE", "apostrophe"},
        {"'", "apostrophe"},
        {"COMMA", "comma"},
        {",", "comma"},
        {"MINUS", "minus"},
        {"-", "minus"},
        {"PERIOD", "period"},
        {".", "period"},
        {"SLASH", "slash"},
        {"/", "slash"},
        {"SEMICOLON", "semicolon"},
        {";", "semicolon"},
        {"EQUALS", "equals"},
        {"=", "equals"},
        {"LEFT_BRACKET", "left-bracket"},
        {"[", "left-bracket"},
        {"BACKSLASH", "backslash"},
        {"\\", "backslash"},
        {"RIGHT_BRACKET", "right-bracket"},
        {"]", "right-bracket"},
        {"GRAVE_ACCENT", "backtick"},
        {"BACKTICK", "backtick"},
        {"`", "backtick"},

        {"ESCAPE", "escape"},
        {"ESC", "escape"},
        {"ENTER", "enter"},
        {"TAB", "tab"},
        {"BACKSPACE", "backspace"},
        {"INSERT", "insert"},
        {"DELETE", "delete"},
        {"DEL", "delete"},
        {"RIGHT", "arrow_right"},
        {"LEFT", "arrow_left"},
        {"DOWN", "arrow_down"},
        {"UP", "arrow_up"},
        {"PAGE_UP", "page-up"},
        {"PAGE_DOWN", "page-down"},
        {"HOME", "home"},
        {"END", "end"},
        {"CAPS_LOCK", "caps-lock"},
        {"CAPSLOCK", "caps-lock"},
        {"SCROLL_LOCK", "scroll-lock"},
        {"SCROLLLOCK", "scroll-lock"},
        {"NUM_LOCK", "num-lock"},
        {"NUMLOCK", "num-lock"},
        {"PRINT_SCREEN", "print-screen"},
        {"PRINTSCREEN", "print-screen"},
        {"PAUSE", "pause"},

        {"F1", "f1"},
        {"F2", "f2"},
        {"F3", "f3"},
        {"F4", "f4"},
        {"F5", "f5"},
        {"F6", "f6"},
        {"F7", "f7"},
        {"F8", "f8"},
        {"F9", "f9"},
        {"F10", "f10"},
        {"F11", "f11"},
        {"F12", "f12"},
        {"F13", "f13"},
        {"F14", "f14"},
        {"F15", "f15"},
        {"F16", "f16"},
        {"F17", "f17"},
        {"F18", "f18"},
        {"F19", "f19"},
        {"F20", "f20"},
        {"F21", "f21"},
        {"F22", "f22"},
        {"F23", "f23"},
        {"F24", "f24"},
        {"F25", "f25"},

        {"NUMPAD_0", "0_numpad"},
        {"NUM_0", "0_numpad"},
        {"NUMPAD_1", "1_numpad"},
        {"NUM_1", "1_numpad"},
        {"NUMPAD_2", "2_numpad"},
        {"NUM_2", "2_numpad"},
        {"NUMPAD_3", "3_numpad"},
        {"NUM_3", "3_numpad"},
        {"NUMPAD_4", "4_numpad"},
        {"NUM_4", "4_numpad"},
        {"NUMPAD_5", "5_numpad"},
        {"NUM_5", "5_numpad"},
        {"NUMPAD_6", "6_numpad"},
        {"NUM_6", "6_numpad"},
        {"NUMPAD_7", "7_numpad"},
        {"NUM_7", "7_numpad"},
        {"NUMPAD_8", "8_numpad"},
        {"NUM_8", "8_numpad"},
        {"NUMPAD_9", "9_numpad"},
        {"NUM_9", "9_numpad"},

        {"NUMPAD_PERIOD", "period_numpad"},
        {"NUM_PERIOD", "period_numpad"},
        {"NUMPAD_DECIMAL", "period_numpad"},
        {"NUM_DECIMAL", "period_numpad"},

        {"NUMPAD_DIVIDE", "divide_numpad"},
        {"NUM_DIVIDE", "divide_numpad"},

        {"NUMPAD_MULTIPLY", "multiply_numpad"},
        {"NUM_MULTIPLY", "multiply_numpad"},

        {"NUMPAD_MINUS", "minus_numpad"},
        {"NUM_MINUS", "minus_numpad"},
        {"NUMPAD_SUBTRACT", "minus_numpad"},
        {"NUM_SUBTRACT", "minus_numpad"},

        {"NUMPAD_PLUS", "plus_numpad"},
        {"NUM_PLUS", "plus_numpad"},
        {"NUMPAD_ADD", "plus_numpad"},
        {"NUM_ADD", "plus_numpad"},

        {"NUMPAD_ENTER", "enter_numpad"},
        {"NUM_ENTER", "enter_numpad"},

        {"NUMPAD_EQUALS", "equals_numpad"},
        {"NUM_EQUALS", "equals_numpad"},

        {"LEFT_SHIFT", "shift_left"},
        {"LEFT_CONTROL", "control_left"},
        {"LEFT_ALT", "alt_left"},
        {"LEFT_SUPER", "super_left"},
        {"RIGHT_SHIFT", "shift_right"},
        {"RIGHT_CONTROL", "control_right"},
        {"RIGHT_ALT", "alt_right"},
        {"RIGHT_SUPER", "super_right"},
    };
} // namespace sp
