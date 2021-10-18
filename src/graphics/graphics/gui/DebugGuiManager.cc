#include "DebugGuiManager.hh"

#include "ecs/EcsImpl.hh"
#include "graphics/gui/ConsoleGui.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>

namespace sp {
    DebugGuiManager::DebugGuiManager() : GuiManager("debug_gui", ecs::FocusLayer::ALWAYS) {
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name>, ecs::Write<ecs::EventInput>>();

        auto gui = guiEntity.Get(lock);
        Assert(gui.Has<ecs::EventInput>(lock), "Expected debug_gui to start with an EventInput");

        auto &eventInput = gui.Get<ecs::EventInput>(lock);
        eventInput.Register(INPUT_EVENT_TOGGLE_CONSOLE);
    }

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

        bool focusChanged = false;
        {
            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::SignalBindings, ecs::SignalOutput, ecs::FocusLayer, ecs::FocusLock>,
                ecs::Write<ecs::EventInput>>();

            auto gui = guiEntity.Get(lock);
            if (gui.Has<ecs::EventInput>(lock)) {
                ecs::Event event;
                while (ecs::EventInput::Poll(lock, gui, INPUT_EVENT_TOGGLE_CONSOLE, event)) {
                    consoleOpen = !consoleOpen;
                }

                auto &prevInput = gui.GetPrevious<ecs::EventInput>(lock);
                if (consoleOpen) {
                    if (!prevInput.IsRegistered(INPUT_EVENT_MENU_TEXT_INPUT)) {
                        gui.Get<ecs::EventInput>(lock).Register(INPUT_EVENT_MENU_TEXT_INPUT);
                    }
                } else {
                    if (prevInput.IsRegistered(INPUT_EVENT_MENU_TEXT_INPUT)) {
                        gui.Get<ecs::EventInput>(lock).Unregister(INPUT_EVENT_MENU_TEXT_INPUT);
                    }
                }
            }

            if (lock.Has<ecs::FocusLock>()) {
                auto &focusLock = lock.Get<ecs::FocusLock>();
                focusChanged = focusLock.HasFocus(ecs::FocusLayer::OVERLAY) != consoleOpen;
            }
        }
        if (focusChanged) {
            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::FocusLock>>();

            auto &focusLock = lock.Get<ecs::FocusLock>();
            if (consoleOpen) {
                focusLock.AcquireFocus(ecs::FocusLayer::OVERLAY);
            } else {
                focusLock.ReleaseFocus(ecs::FocusLayer::OVERLAY);
            }
        }
    }
} // namespace sp
