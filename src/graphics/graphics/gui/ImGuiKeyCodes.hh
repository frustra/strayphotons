#pragma once

#include "input/KeyCodes.hh"

#include <imgui/imgui.h>
#include <robin_hood.h>

namespace sp {
    static const robin_hood::unordered_flat_map<KeyCode, ImGuiKey> ImGuiKeyMapping = {
        {KEY_SPACE, ImGuiKey_Space},
        {KEY_APOSTROPHE, ImGuiKey_Apostrophe},
        {KEY_COMMA, ImGuiKey_Comma},
        {KEY_MINUS, ImGuiKey_Minus},
        {KEY_PERIOD, ImGuiKey_Period},
        {KEY_SLASH, ImGuiKey_Slash},
        {KEY_0, ImGuiKey_0},
        {KEY_1, ImGuiKey_1},
        {KEY_2, ImGuiKey_2},
        {KEY_3, ImGuiKey_3},
        {KEY_4, ImGuiKey_4},
        {KEY_5, ImGuiKey_5},
        {KEY_6, ImGuiKey_6},
        {KEY_7, ImGuiKey_7},
        {KEY_8, ImGuiKey_8},
        {KEY_9, ImGuiKey_9},
        {KEY_SEMICOLON, ImGuiKey_Semicolon},
        {KEY_EQUALS, ImGuiKey_Equal},
        {KEY_A, ImGuiKey_A},
        {KEY_B, ImGuiKey_B},
        {KEY_C, ImGuiKey_C},
        {KEY_D, ImGuiKey_D},
        {KEY_E, ImGuiKey_E},
        {KEY_F, ImGuiKey_F},
        {KEY_G, ImGuiKey_G},
        {KEY_H, ImGuiKey_H},
        {KEY_I, ImGuiKey_I},
        {KEY_J, ImGuiKey_J},
        {KEY_K, ImGuiKey_K},
        {KEY_L, ImGuiKey_L},
        {KEY_M, ImGuiKey_M},
        {KEY_N, ImGuiKey_N},
        {KEY_O, ImGuiKey_O},
        {KEY_P, ImGuiKey_P},
        {KEY_Q, ImGuiKey_Q},
        {KEY_R, ImGuiKey_R},
        {KEY_S, ImGuiKey_S},
        {KEY_T, ImGuiKey_T},
        {KEY_U, ImGuiKey_U},
        {KEY_V, ImGuiKey_V},
        {KEY_W, ImGuiKey_W},
        {KEY_X, ImGuiKey_X},
        {KEY_Y, ImGuiKey_Y},
        {KEY_Z, ImGuiKey_Z},
        {KEY_LEFT_BRACKET, ImGuiKey_LeftBracket},
        {KEY_BACKSLASH, ImGuiKey_Backslash},
        {KEY_RIGHT_BRACKET, ImGuiKey_RightBracket},
        {KEY_BACKTICK, ImGuiKey_GraveAccent},
        {KEY_ESCAPE, ImGuiKey_Escape},
        {KEY_ENTER, ImGuiKey_Enter},
        {KEY_TAB, ImGuiKey_Tab},
        {KEY_BACKSPACE, ImGuiKey_Backspace},
        {KEY_INSERT, ImGuiKey_Insert},
        {KEY_DELETE, ImGuiKey_Delete},
        {KEY_RIGHT_ARROW, ImGuiKey_RightArrow},
        {KEY_LEFT_ARROW, ImGuiKey_LeftArrow},
        {KEY_DOWN_ARROW, ImGuiKey_DownArrow},
        {KEY_UP_ARROW, ImGuiKey_UpArrow},
        {KEY_PAGE_UP, ImGuiKey_PageUp},
        {KEY_PAGE_DOWN, ImGuiKey_PageDown},
        {KEY_HOME, ImGuiKey_Home},
        {KEY_END, ImGuiKey_End},
        {KEY_CAPS_LOCK, ImGuiKey_CapsLock},
        {KEY_SCROLL_LOCK, ImGuiKey_ScrollLock},
        {KEY_NUM_LOCK, ImGuiKey_NumLock},
        {KEY_PRINT_SCREEN, ImGuiKey_PrintScreen},
        {KEY_PAUSE, ImGuiKey_Pause},
        {KEY_F1, ImGuiKey_F1},
        {KEY_F2, ImGuiKey_F2},
        {KEY_F3, ImGuiKey_F3},
        {KEY_F4, ImGuiKey_F4},
        {KEY_F5, ImGuiKey_F5},
        {KEY_F6, ImGuiKey_F6},
        {KEY_F7, ImGuiKey_F7},
        {KEY_F8, ImGuiKey_F8},
        {KEY_F9, ImGuiKey_F9},
        {KEY_F10, ImGuiKey_F10},
        {KEY_F11, ImGuiKey_F11},
        {KEY_F12, ImGuiKey_F12},
        {KEY_0_NUMPAD, ImGuiKey_Keypad0},
        {KEY_1_NUMPAD, ImGuiKey_Keypad1},
        {KEY_2_NUMPAD, ImGuiKey_Keypad2},
        {KEY_3_NUMPAD, ImGuiKey_Keypad3},
        {KEY_4_NUMPAD, ImGuiKey_Keypad4},
        {KEY_5_NUMPAD, ImGuiKey_Keypad5},
        {KEY_6_NUMPAD, ImGuiKey_Keypad6},
        {KEY_7_NUMPAD, ImGuiKey_Keypad7},
        {KEY_8_NUMPAD, ImGuiKey_Keypad8},
        {KEY_9_NUMPAD, ImGuiKey_Keypad9},
        {KEY_DECIMAL_NUMPAD, ImGuiKey_KeypadDecimal},
        {KEY_DIVIDE_NUMPAD, ImGuiKey_KeypadDivide},
        {KEY_MULTIPLY_NUMPAD, ImGuiKey_KeypadMultiply},
        {KEY_MINUS_NUMPAD, ImGuiKey_KeypadSubtract},
        {KEY_PLUS_NUMPAD, ImGuiKey_KeypadAdd},
        {KEY_ENTER_NUMPAD, ImGuiKey_KeypadEnter},
        {KEY_EQUALS_NUMPAD, ImGuiKey_KeypadEqual},
        {KEY_LEFT_SHIFT, ImGuiKey_LeftShift},
        {KEY_LEFT_CONTROL, ImGuiKey_LeftCtrl},
        {KEY_LEFT_ALT, ImGuiKey_LeftAlt},
        {KEY_LEFT_SUPER, ImGuiKey_LeftSuper},
        {KEY_RIGHT_SHIFT, ImGuiKey_RightShift},
        {KEY_RIGHT_CONTROL, ImGuiKey_RightCtrl},
        {KEY_RIGHT_ALT, ImGuiKey_RightAlt},
        {KEY_RIGHT_SUPER, ImGuiKey_RightSuper},
        {KEY_CONTEXT_MENU, ImGuiKey_Menu},
    };
} // namespace sp