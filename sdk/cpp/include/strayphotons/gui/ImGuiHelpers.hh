/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "imgui_internal.h"
#include "strayphotons/Logging.hh"
#include "strayphotons/gui/GuiDrawData.hh"
#include "strayphotons/gui/GuiInputEvents.hh"
#include "strayphotons/gui/ImGuiKeyCodes.hh"
#include "strayphotons/input/KeyCodes.hh"

#include <imgui.h>

#ifdef SP_SHARED_INTERNAL
typedef sp::GuiDrawData sp_gui_draw_data_t;
typedef sp::GuiInputEvent sp_gui_input_event_t;
typedef sp::GuiInputSource sp_gui_input_source_t;
typedef sp::GuiMouseSource sp_gui_mouse_source_t;
#else
    #include <strayphotons/components_gen.h>
#endif

namespace sp {
    static_assert(sizeof(ImDrawVert) == sizeof(GuiDrawVertex), "ImDrawVert size does not match");
    static_assert(offsetof(ImDrawVert, pos) == offsetof(GuiDrawVertex, pos), "ImDrawVert pos offset does not match");
    static_assert(offsetof(ImDrawVert, uv) == offsetof(GuiDrawVertex, uv), "ImDrawVert uv offset does not match");
    static_assert(offsetof(ImDrawVert, col) == offsetof(GuiDrawVertex, col), "ImDrawVert col offset does not match");

    static inline void ConvertImDrawData(const ImDrawData *drawData, sp_gui_draw_data_t &output) {
        Assertf(drawData != nullptr, "GuiDrawData being constructed with null ImDrawData");

        size_t totalCommandCount = 0, totalIndexCount = 0, totalVertexCount = 0;
        for (const auto &cmdList : drawData->CmdLists) {
            totalCommandCount += cmdList->CmdBuffer.size();
            totalIndexCount += cmdList->IdxBuffer.size();
            totalVertexCount += cmdList->VtxBuffer.size();
        }
#ifdef SP_SHARED_INTERNAL
        output.drawCommands.resize(totalCommandCount);
        output.indexBuffer.resize(totalIndexCount);
        output.vertexBuffer.resize(totalVertexCount);
        if (totalCommandCount == 0 || totalIndexCount == 0 || totalVertexCount == 0) return;

        GuiDrawVertex *vertexIter = output.vertexBuffer.data();
        GuiDrawIndex *indexIter = output.indexBuffer.data();
        GuiDrawCommand *cmdIter = output.drawCommands.data();
#else
        sp_gui_draw_command_vector_resize(&output.draw_commands, totalCommandCount);
        sp_uint16_vector_resize(&output.index_buffer, totalIndexCount);
        sp_gui_draw_vertex_vector_resize(&output.vertex_buffer, totalVertexCount);
        if (totalCommandCount == 0 || totalIndexCount == 0 || totalVertexCount == 0) return;

        auto *vertexIter = reinterpret_cast<GuiDrawVertex *>(sp_gui_draw_vertex_vector_get_data(&output.vertex_buffer));
        auto *indexIter = reinterpret_cast<GuiDrawIndex *>(sp_uint16_vector_get_data(&output.index_buffer));
        auto *cmdIter = reinterpret_cast<GuiDrawCommand *>(sp_gui_draw_command_vector_get_data(&output.draw_commands));
#endif

        for (const auto &cmdList : drawData->CmdLists) {
            GuiDrawVertex *begin = reinterpret_cast<GuiDrawVertex *>(cmdList->VtxBuffer.Data);
            std::copy(begin, begin + cmdList->VtxBuffer.size(), vertexIter);
            vertexIter += cmdList->VtxBuffer.size();
            std::copy(cmdList->IdxBuffer.begin(), cmdList->IdxBuffer.end(), indexIter);
            indexIter += cmdList->IdxBuffer.size();
        }

        uint32_t vertexOffset = 0;
        for (const auto &cmdList : drawData->CmdLists) {
            for (const auto &cmd : cmdList->CmdBuffer) {
                Assertf(!cmd.UserCallback, "GuiDrawData UserCallback is not supported");

                cmdIter->clipRect = {cmd.ClipRect.x - drawData->DisplayPos.x,
                    cmd.ClipRect.y - drawData->DisplayPos.y,
                    cmd.ClipRect.z - drawData->DisplayPos.x,
                    cmd.ClipRect.w - drawData->DisplayPos.y};
                cmdIter->textureId = (uint64_t)cmd.TextureId;
                cmdIter->indexCount = cmd.ElemCount;
                cmdIter->vertexOffset = vertexOffset + cmd.VtxOffset;
                cmdIter++;
            }
            vertexOffset += cmdList->VtxBuffer.size();
        }
    }

    static inline sp_gui_input_source_t ConvertImGuiInputSource(ImGuiInputSource source) {
        switch (source) {
        case ImGuiInputSource_Mouse:
            return (sp_gui_input_source_t)GuiInputSource::Mouse;
        case ImGuiInputSource_Keyboard:
            return (sp_gui_input_source_t)GuiInputSource::Keyboard;
        case ImGuiInputSource_Gamepad:
            return (sp_gui_input_source_t)GuiInputSource::Gamepad;
        default:
            return (sp_gui_input_source_t)GuiInputSource::None;
        }
    }

    static inline ImGuiInputSource ConvertImGuiInputSource(sp_gui_input_source_t source) {
#ifdef SP_SHARED_INTERNAL
        switch (source) {
        case GuiInputSource::Mouse:
            return ImGuiInputSource_Mouse;
        case GuiInputSource::Keyboard:
            return ImGuiInputSource_Keyboard;
        case GuiInputSource::Gamepad:
            return ImGuiInputSource_Gamepad;
        default:
            return ImGuiInputSource_None;
        }
#else
        switch (source) {
        case SP_GUI_INPUT_SOURCE_MOUSE:
            return ImGuiInputSource_Mouse;
        case SP_GUI_INPUT_SOURCE_KEYBOARD:
            return ImGuiInputSource_Keyboard;
        case SP_GUI_INPUT_SOURCE_GAMEPAD:
            return ImGuiInputSource_Gamepad;
        default:
            return ImGuiInputSource_None;
        }
#endif
    }

    static inline sp_gui_mouse_source_t ConvertImGuiMouseSource(ImGuiMouseSource source) {
        switch (source) {
        case ImGuiMouseSource_Mouse:
            return (sp_gui_mouse_source_t)GuiMouseSource::Mouse;
        case ImGuiMouseSource_TouchScreen:
            return (sp_gui_mouse_source_t)GuiMouseSource::TouchScreen;
        case ImGuiMouseSource_Pen:
            return (sp_gui_mouse_source_t)GuiMouseSource::Pen;
        default:
            return (sp_gui_mouse_source_t)GuiMouseSource::Mouse;
        }
    }

    static inline ImGuiMouseSource ConvertImGuiMouseSource(sp_gui_mouse_source_t source) {
#ifdef SP_SHARED_INTERNAL
        switch (source) {
        case GuiMouseSource::Mouse:
            return ImGuiMouseSource_Mouse;
        case GuiMouseSource::TouchScreen:
            return ImGuiMouseSource_TouchScreen;
        case GuiMouseSource::Pen:
            return ImGuiMouseSource_Pen;
        default:
            return ImGuiMouseSource_Mouse;
        }
#else
        switch (source) {
        case SP_GUI_MOUSE_SOURCE_MOUSE:
            return ImGuiMouseSource_Mouse;
        case SP_GUI_MOUSE_SOURCE_TOUCH_SCREEN:
            return ImGuiMouseSource_TouchScreen;
        case SP_GUI_MOUSE_SOURCE_PEN:
            return ImGuiMouseSource_Pen;
        default:
            return ImGuiMouseSource_Mouse;
        }
#endif
    }

    static inline sp_gui_input_event_t ConvertImGuiInputEvent(const ImGuiInputEvent &event) {
        sp_gui_input_event_t output = {};
        output.source = ConvertImGuiInputSource(event.Source);
#ifdef SP_SHARED_INTERNAL
        output.eventId = event.EventId;
        switch (event.Type) {
        case ImGuiInputEventType_MousePos:
            output.type = GuiInputEventType::MousePos;
            output.mouseSource = ConvertImGuiMouseSource(event.MousePos.MouseSource);
            output.mousePos = glm::vec2(event.MousePos.PosX, event.MousePos.PosY);
            return output;
        case ImGuiInputEventType_MouseWheel:
            output.type = GuiInputEventType::MouseWheel;
            output.mouseSource = ConvertImGuiMouseSource(event.MousePos.MouseSource);
            output.mouseWheel = glm::vec2(event.MouseWheel.WheelX, event.MouseWheel.WheelY);
            return output;
        case ImGuiInputEventType_MouseButton:
            output.type = GuiInputEventType::MouseButton;
            output.mouseSource = ConvertImGuiMouseSource(event.MousePos.MouseSource);
            output.mouseButton = GuiInputMouseButtonEvent{
                event.MouseButton.Button,
                event.MouseButton.Down ? InputAction::PRESS : InputAction::RELEASE,
            };
            return output;
        case ImGuiInputEventType_Key:
            output.type = GuiInputEventType::Key;
            output.key = GuiInputKeyEvent{
                (int)event.Key.Key,
                event.Key.Down ? InputAction::PRESS : InputAction::RELEASE,
                event.Key.AnalogValue,
            };
            return output;
        case ImGuiInputEventType_Text:
            output.type = GuiInputEventType::Text;
            output.text = event.Text.Char;
            return output;
        case ImGuiInputEventType_Focus:
            output.type = GuiInputEventType::Focus;
            output.appFocused = event.AppFocused.Focused;
            return output;
        default:
            output.type = GuiInputEventType::None;
            return output;
        }
#else
        output.event_id = event.EventId;
        switch (event.Type) {
        case ImGuiInputEventType_MousePos:
            output.type = SP_GUI_INPUT_EVENT_TYPE_MOUSE_POS;
            output.source = ConvertImGuiMouseSource(event.MousePos.MouseSource);
            output.mouse_pos_event = vec2_t{event.MousePos.PosX, event.MousePos.PosY};
            return output;
        case ImGuiInputEventType_MouseWheel:
            output.type = SP_GUI_INPUT_EVENT_TYPE_MOUSE_WHEEL;
            output.source = ConvertImGuiMouseSource(event.MousePos.MouseSource);
            output.mouse_wheel_event = vec2_t{event.MouseWheel.WheelX, event.MouseWheel.WheelY};
            return output;
        case ImGuiInputEventType_MouseButton:
            output.type = SP_GUI_INPUT_EVENT_TYPE_MOUSE_BUTTON;
            output.source = ConvertImGuiMouseSource(event.MousePos.MouseSource);
            output.mouse_button_event = sp_gui_input_mouse_button_event_t{
                event.MouseButton.Button,
                event.MouseButton.Down ? SP_INPUT_ACTION_PRESS : SP_INPUT_ACTION_RELEASE,
            };
            return output;
        case ImGuiInputEventType_Key:
            output.type = SP_GUI_INPUT_EVENT_TYPE_KEY;
            output.key_event = sp_gui_input_key_event_t{
                (int)event.Key.Key,
                event.Key.Down ? SP_INPUT_ACTION_PRESS : SP_INPUT_ACTION_RELEASE,
                event.Key.AnalogValue,
            };
            return output;
        case ImGuiInputEventType_Text:
            output.type = SP_GUI_INPUT_EVENT_TYPE_TEXT;
            output.text_event = event.Text.Char;
            return output;
        case ImGuiInputEventType_Focus:
            output.type = SP_GUI_INPUT_EVENT_TYPE_FOCUS;
            output.app_focused_event = event.AppFocused.Focused;
            return output;
        default:
            output.type = SP_GUI_INPUT_EVENT_TYPE_NONE;
            return output;
        }
#endif
    }

    static inline ImGuiInputEvent ConvertImGuiInputEvent(const sp_gui_input_event_t &event) {
        ImGuiInputEvent output = {};
        output.Source = ConvertImGuiInputSource(event.source);
#ifdef SP_SHARED_INTERNAL
        output.EventId = event.eventId;
        switch (event.type) {
        case GuiInputEventType::MousePos:
            output.Type = ImGuiInputEventType_MousePos;
            output.MousePos = ImGuiInputEventMousePos{
                event.mousePos.x,
                event.mousePos.y,
                ConvertImGuiMouseSource(event.mouseSource),
            };
            return output;
        case GuiInputEventType::MouseWheel:
            output.Type = ImGuiInputEventType_MouseWheel;
            output.MouseWheel = ImGuiInputEventMouseWheel{
                event.mouseWheel.x,
                event.mouseWheel.y,
                ConvertImGuiMouseSource(event.mouseSource),
            };
            return output;
        case GuiInputEventType::MouseButton:
            output.Type = ImGuiInputEventType_MouseButton;
            output.MouseButton = ImGuiInputEventMouseButton{
                event.mouseButton.button,
                event.mouseButton.action == InputAction::PRESS,
                ConvertImGuiMouseSource(event.mouseSource),
            };
            return output;
        case GuiInputEventType::Key:
            output.Type = ImGuiInputEventType_Key;
            output.Key = ImGuiInputEventKey{
                (ImGuiKey)event.key.key,
                event.key.action == InputAction::PRESS,
                event.key.analogValue,
            };
            return output;
        case GuiInputEventType::Text:
            output.Type = ImGuiInputEventType_Text;
            output.Text.Char = event.text;
            return output;
        case GuiInputEventType::Focus:
            output.Type = ImGuiInputEventType_Focus;
            output.AppFocused.Focused = event.appFocused;
            return output;
        default:
            output.Type = ImGuiInputEventType_None;
            return output;
        }
#else
        output.EventId = event.event_id;
        switch (event.type) {
        case ImGuiInputEventType_MousePos:
            output.Type = ImGuiInputEventType_MousePos;
            output.MousePos = ImGuiInputEventMousePos{
                event.mouse_pos_event.v[0],
                event.mouse_pos_event.v[1],
                ConvertImGuiMouseSource(event.mouse_source),
            };
            return output;
        case ImGuiInputEventType_MouseWheel:
            output.Type = ImGuiInputEventType_MouseWheel;
            output.MouseWheel = ImGuiInputEventMouseWheel{
                event.mouse_wheel_event.v[0],
                event.mouse_wheel_event.v[1],
                ConvertImGuiMouseSource(event.mouse_source),
            };
            return output;
        case ImGuiInputEventType_MouseButton:
            output.Type = ImGuiInputEventType_MouseButton;
            output.MouseButton = ImGuiInputEventMouseButton{
                event.mouse_button_event.button,
                event.mouse_button_event.action == SP_INPUT_ACTION_PRESS,
                ConvertImGuiMouseSource(event.mouse_source),
            };
            return output;
        case ImGuiInputEventType_Key:
            output.Type = ImGuiInputEventType_Key;
            output.Key = ImGuiInputEventKey{
                (ImGuiKey)event.key_event.key,
                event.key_event.action == SP_INPUT_ACTION_PRESS,
                event.key_event.analog_value,
            };
            return output;
        case ImGuiInputEventType_Text:
            output.Type = ImGuiInputEventType_Text;
            output.Text.Char = event.text_event;
            return output;
        case ImGuiInputEventType_Focus:
            output.Type = ImGuiInputEventType_Focus;
            output.AppFocused.Focused = event.app_focused_event;
            return output;
        default:
            output.Type = ImGuiInputEventType_None;
            return output;
        }
#endif
    }
} // namespace sp
