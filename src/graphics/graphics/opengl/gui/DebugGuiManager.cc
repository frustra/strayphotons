#include "DebugGuiManager.hh"

#include "GraphicsManager.hh"
#include "graphics/opengl/gui/ConsoleGui.hh"
#include "input/InputManager.hh"

#include <imgui/imgui.h>

namespace sp {
    void DebugGuiManager::DefineWindows() {
        SetGuiContext();
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

        static ConsoleGui console;
        if (consoleOpen) console.Add();
        GuiManager::DefineWindows();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    void DebugGuiManager::BeforeFrame() {
        GuiManager::BeforeFrame();

        ImGui::StyleColorsClassic();

        ImGuiIO &io = ImGui::GetIO();
        io.MouseDrawCursor = false;

        if (input.IsPressed(INPUT_ACTION_TOGGLE_CONSOLE)) { ToggleConsole(); }

        if (Focused() && !input.FocusLocked(focusPriority)) {
            io.MouseDown[0] = input.IsDown(INPUT_ACTION_MOUSE_BASE + "/button_left");
            io.MouseDown[1] = input.IsDown(INPUT_ACTION_MOUSE_BASE + "/button_right");
            io.MouseDown[2] = input.IsDown(INPUT_ACTION_MOUSE_BASE + "/button_middle");

            const glm::vec2 *scrollOffset, *scrollOffsetPrev;
            if (input.GetActionDelta(INPUT_ACTION_MOUSE_SCROLL, &scrollOffset, &scrollOffsetPrev)) {
                if (scrollOffsetPrev != nullptr) {
                    io.MouseWheel = scrollOffset->y - scrollOffsetPrev->y;
                } else {
                    io.MouseWheel = scrollOffset->y;
                }
            }

            const glm::vec2 *mousePos;
            if (input.GetActionValue(INPUT_ACTION_MOUSE_CURSOR, &mousePos)) {
                io.MousePos = ImVec2(mousePos->x, mousePos->y);
            }

            const CharEvents *chars;
            if (input.GetActionValue(INPUT_ACTION_KEYBOARD_CHARS, &chars)) {
                for (auto &ch : *chars) {
                    if (ch > 0 && ch < 0x10000) io.AddInputCharacter(ch);
                }
            }
        }
    }

    void DebugGuiManager::ToggleConsole() {
        consoleOpen = !consoleOpen;

        if (consoleOpen) {
            input.LockFocus(true, focusPriority);
            graphics.EnableCursor();
        } else {
            graphics.DisableCursor();
            input.LockFocus(false, focusPriority);
        }
    }
} // namespace sp
