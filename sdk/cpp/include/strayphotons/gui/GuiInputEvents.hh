/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "strayphotons/HeapVector.hh"
#include "strayphotons/input/KeyCodes.hh"

#include <glm/glm.hpp>

namespace sp {
    enum class GuiInputEventType : uint16_t {
        None = 0,
        MousePos,
        MouseWheel,
        MouseButton,
        Key,
        Text,
        Focus,
    };

    enum class GuiInputSource : uint8_t {
        None = 0,
        Mouse,
        Keyboard,
        Gamepad,
    };

    enum class GuiMouseSource : uint8_t {
        Mouse = 0,
        TouchScreen,
        Pen,
    };

    struct GuiInputMouseButtonEvent {
        int button; // ImGuiMouseButton
        InputAction action;
    };

    struct GuiInputKeyEvent {
        int key; // ImGuiKey
        InputAction action;
        float analogValue;
    };

    struct GuiInputEvent {
        GuiInputEventType type;
        GuiInputSource source;
        GuiMouseSource mouseSource;
        uint32_t eventId;
        union {
            glm::vec2 mousePos;
            glm::vec2 mouseWheel;
            GuiInputMouseButtonEvent mouseButton;
            GuiInputKeyEvent key;
            unsigned int text;
            bool appFocused;
        };

        bool operator==(const GuiInputEvent &other) const {
            if (type != other.type || source != other.source || eventId != other.eventId) return false;
            switch (type) {
            case GuiInputEventType::None:
                return true;
            case GuiInputEventType::MousePos:
                return mousePos == other.mousePos && mouseSource == other.mouseSource;
            case GuiInputEventType::MouseWheel:
                return mouseWheel == other.mouseWheel && mouseSource == other.mouseSource;
            case GuiInputEventType::MouseButton:
                return mouseButton.button == other.mouseButton.button &&
                       mouseButton.action == other.mouseButton.action && mouseSource == other.mouseSource;
            case GuiInputEventType::Key:
                return key.key == other.key.key && key.action == other.key.action &&
                       key.analogValue == other.key.analogValue;
            case GuiInputEventType::Text:
                return text == other.text;
            case GuiInputEventType::Focus:
                return appFocused == other.appFocused;
            default:
                Abortf("Unknown GuiInputEvent type: %s", type);
            }
        }
    };
} // namespace sp
