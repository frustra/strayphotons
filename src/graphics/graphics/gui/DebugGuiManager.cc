#include "DebugGuiManager.hh"

#include "ecs/EcsImpl.hh"
#include "graphics/gui/ConsoleGui.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>

namespace sp {
    DebugGuiManager::DebugGuiManager() : SystemGuiManager("debug", ecs::FocusLayer::Always) {
        auto lock = ecs::StartTransaction<ecs::AddRemove>();

        auto gui = guiEntity.Get(lock);
        Assert(gui.Has<ecs::EventInput>(lock), "Expected debug gui to start with an EventInput");

        auto &eventInput = gui.Get<ecs::EventInput>(lock);
        eventInput.Register(INPUT_EVENT_TOGGLE_CONSOLE);

        guiObserver = lock.Watch<ecs::ComponentEvent<ecs::Gui>>();

        for (auto &ent : lock.EntitiesWith<ecs::Gui>()) {
            guis.emplace_back(GuiEntityContext{ent});
        }
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

        for (auto &component : components) {
            bool isWindow = dynamic_cast<GuiWindow *>(component.get()) != nullptr;

            if (isWindow) ImGui::Begin(component->name.c_str(), nullptr, {});
            component->DefineContents();
            if (isWindow) ImGui::End();
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    void DebugGuiManager::BeforeFrame() {
        SystemGuiManager::BeforeFrame();

        ImGui::StyleColorsClassic();

        ImGuiIO &io = ImGui::GetIO();
        io.MouseDrawCursor = false;

        bool focusChanged = false;
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Read<ecs::EventInput, ecs::Gui>>();

            auto thisEntity = guiEntity.Get(lock);
            if (thisEntity.Has<ecs::EventInput>(lock)) {
                ecs::Event event;
                while (ecs::EventInput::Poll(lock, thisEntity, INPUT_EVENT_TOGGLE_CONSOLE, event)) {
                    consoleOpen = !consoleOpen;
                }
            }

            if (lock.Has<ecs::FocusLock>()) {
                auto &focusLock = lock.Get<ecs::FocusLock>();
                focusChanged = focusLock.HasFocus(ecs::FocusLayer::Overlay) != consoleOpen;
            }

            ecs::ComponentEvent<ecs::Gui> guiEvent;
            while (guiObserver.Poll(lock, guiEvent)) {
                auto &eventEntity = guiEvent.entity;

                if (guiEvent.type == Tecs::EventType::REMOVED) {
                    for (auto it = guis.begin(); it != guis.end(); it++) {
                        if (it->entity == eventEntity) {
                            guis.erase(it);
                            break;
                        }
                    }
                } else if (guiEvent.type == Tecs::EventType::ADDED) {
                    if (!eventEntity.Has<ecs::Gui>(lock)) continue;
                    guis.emplace_back(GuiEntityContext{eventEntity});
                }
            }

            for (auto &ctx : guis) {
                Assert(ctx.entity.Has<ecs::Gui>(lock), "gui entity must have a gui component");

                auto &gui = ctx.entity.Get<ecs::Gui>(lock);
                if (gui.target == ecs::GuiTarget::Debug) {
                    if (!ctx.window && !gui.windowName.empty()) {
                        ctx.window = CreateGuiWindow(this, gui.windowName);
                    }
                } else if (ctx.window) {
                    Detach(ctx.window);
                    ctx.window.reset();
                }
            }
        }
        if (focusChanged) {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::FocusLock, ecs::EventInput>>();
            auto gui = guiEntity.Get(lock);

            auto &focusLock = lock.Get<ecs::FocusLock>();
            auto &eventInput = gui.Get<ecs::EventInput>(lock);

            if (consoleOpen) {
                focusLock.AcquireFocus(ecs::FocusLayer::Overlay);
                if (!eventInput.IsRegistered(INPUT_EVENT_MENU_TEXT_INPUT)) {
                    eventInput.Register(INPUT_EVENT_MENU_TEXT_INPUT);
                }
            } else {
                focusLock.ReleaseFocus(ecs::FocusLayer::Overlay);
                if (eventInput.IsRegistered(INPUT_EVENT_MENU_TEXT_INPUT)) {
                    eventInput.Unregister(INPUT_EVENT_MENU_TEXT_INPUT);
                }
            }
        }
    }
} // namespace sp
