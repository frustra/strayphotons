//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//       Key definitions
//

#pragma once

//
// DOOM keyboard definition.
// This is the stuff configured by Setup.Exe.
// Most key data are simple ascii (uppercased).
//
#define DOOMKEY_RIGHTARROW 0xae
#define DOOMKEY_LEFTARROW 0xac
#define DOOMKEY_UPARROW 0xad
#define DOOMKEY_DOWNARROW 0xaf
#define DOOMKEY_STRAFE_L 0xa0
#define DOOMKEY_STRAFE_R 0xa1
#define DOOMKEY_USE 0xa2
#define DOOMKEY_FIRE 0xa3
#define DOOMKEY_ESCAPE 27
#define DOOMKEY_ENTER 13
#define DOOMKEY_TAB 9
#define DOOMKEY_F1 (0x80 + 0x3b)
#define DOOMKEY_F2 (0x80 + 0x3c)
#define DOOMKEY_F3 (0x80 + 0x3d)
#define DOOMKEY_F4 (0x80 + 0x3e)
#define DOOMKEY_F5 (0x80 + 0x3f)
#define DOOMKEY_F6 (0x80 + 0x40)
#define DOOMKEY_F7 (0x80 + 0x41)
#define DOOMKEY_F8 (0x80 + 0x42)
#define DOOMKEY_F9 (0x80 + 0x43)
#define DOOMKEY_F10 (0x80 + 0x44)
#define DOOMKEY_F11 (0x80 + 0x57)
#define DOOMKEY_F12 (0x80 + 0x58)

#define DOOMKEY_BACKSPACE 0x7f
#define DOOMKEY_PAUSE 0xff

#define DOOMKEY_EQUALS 0x3d
#define DOOMKEY_MINUS 0x2d

#define DOOMKEY_RSHIFT (0x80 + 0x36)
#define DOOMKEY_RCTRL (0x80 + 0x1d)
#define DOOMKEY_RALT (0x80 + 0x38)

#define DOOMKEY_LALT DOOMKEY_RALT

// new keys:

#define DOOMKEY_CAPSLOCK (0x80 + 0x3a)
#define DOOMKEY_NUMLOCK (0x80 + 0x45)
#define DOOMKEY_SCRLCK (0x80 + 0x46)
#define DOOMKEY_PRTSCR (0x80 + 0x59)

#define DOOMKEY_HOME (0x80 + 0x47)
#define DOOMKEY_END (0x80 + 0x4f)
#define DOOMKEY_PGUP (0x80 + 0x49)
#define DOOMKEY_PGDN (0x80 + 0x51)
#define DOOMKEY_INS (0x80 + 0x52)
#define DOOMKEY_DEL (0x80 + 0x53)

#define DOOMKEYP_0 0
#define DOOMKEYP_1 DOOMKEY_END
#define DOOMKEYP_2 DOOMKEY_DOWNARROW
#define DOOMKEYP_3 DOOMKEY_PGDN
#define DOOMKEYP_4 DOOMKEY_LEFTARROW
#define DOOMKEYP_5 '5'
#define DOOMKEYP_6 DOOMKEY_RIGHTARROW
#define DOOMKEYP_7 DOOMKEY_HOME
#define DOOMKEYP_8 DOOMKEY_UPARROW
#define DOOMKEYP_9 DOOMKEY_PGUP

#define DOOMKEYP_DIVIDE '/'
#define DOOMKEYP_PLUS '+'
#define DOOMKEYP_MINUS '-'
#define DOOMKEYP_MULTIPLY '*'
#define DOOMKEYP_PERIOD 0
#define DOOMKEYP_EQUALS DOOMKEY_EQUALS
#define DOOMKEYP_ENTER DOOMKEY_ENTER

#include <common/Logging.hh>
#include <input/KeyCodes.hh>

static unsigned char convertToDoomKey(sp::KeyCode keyCode) {
    switch (keyCode) {
    case sp::KEY_ENTER:
        return DOOMKEY_ENTER;
    case sp::KEY_ESCAPE:
        return DOOMKEY_ESCAPE;
    case sp::KEY_LEFT_ARROW:
        return DOOMKEY_LEFTARROW;
    case sp::KEY_RIGHT_ARROW:
        return DOOMKEY_RIGHTARROW;
    case sp::KEY_UP_ARROW:
        return DOOMKEY_UPARROW;
    case sp::KEY_DOWN_ARROW:
        return DOOMKEY_DOWNARROW;
    case sp::KEY_LEFT_CONTROL:
    case sp::KEY_RIGHT_CONTROL:
        return DOOMKEY_FIRE;
    case sp::KEY_SPACE:
        return DOOMKEY_USE;
    case sp::KEY_LEFT_SHIFT:
    case sp::KEY_RIGHT_SHIFT:
        return DOOMKEY_RSHIFT;
    case sp::KEY_LEFT_ALT:
    case sp::KEY_RIGHT_ALT:
        return DOOMKEY_LALT;
    case sp::KEY_F2:
        return DOOMKEY_F2;
    case sp::KEY_F3:
        return DOOMKEY_F3;
    case sp::KEY_F4:
        return DOOMKEY_F4;
    case sp::KEY_F5:
        return DOOMKEY_F5;
    case sp::KEY_F6:
        return DOOMKEY_F6;
    case sp::KEY_F7:
        return DOOMKEY_F7;
    case sp::KEY_F8:
        return DOOMKEY_F8;
    case sp::KEY_F9:
        return DOOMKEY_F9;
    case sp::KEY_F10:
        return DOOMKEY_F10;
    case sp::KEY_F11:
        return DOOMKEY_F11;
    case sp::KEY_EQUALS:
    case sp::KEY_PLUS_NUMPAD:
        return DOOMKEY_EQUALS;
    case sp::KEY_MINUS:
    case sp::KEY_MINUS_NUMPAD:
        return DOOMKEY_MINUS;
    default:
        auto it = sp::KeycodeNameLookup.find(keyCode);
        Assertf(it != sp::KeycodeNameLookup.end(), "Unknown DOOMKEY keycode: %s", keyCode);
        if (it->second.length() > 1) {
            for (auto &[alias, binding] : sp::UserBindingAliases) {
                if (alias.length() == 1 && it->second == binding) {
                    return tolower(alias[0]);
                }
            }
            Logf("Unsupported DOOMKEY %s", it->second);
            return 0;
        }
        return tolower(it->second[0]);
    }
}
