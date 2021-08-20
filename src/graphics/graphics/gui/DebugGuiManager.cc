#include "DebugGuiManager.hh"

#include "GraphicsManager.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/gui/ConsoleGui.hh"
#include "input/core/BindingNames.hh"

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

        // TODO: Handle focus
        // if (Focused() && !input.FocusLocked(focusPriority))
        {
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::SignalBindings, ecs::SignalOutput>,
                                                    ecs::Write<ecs::EventInput>>();

            auto player = playerEntity.Get(lock);
            if (player.Has<ecs::EventInput>(lock)) {
                ecs::Event event;
                while (ecs::EventInput::Poll(lock, player, INPUT_EVENT_TOGGLE_CONSOLE, event)) {
                    ToggleConsole();
                }
            }
        }
    }

    void DebugGuiManager::ToggleConsole() {
        consoleOpen = !consoleOpen;

        if (consoleOpen) {
            // input.LockFocus(true, focusPriority);
            graphics.EnableCursor();
        } else {
            graphics.DisableCursor();
            // input.LockFocus(false, focusPriority);
        }
    }
} // namespace sp
