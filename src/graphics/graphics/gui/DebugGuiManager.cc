#include "DebugGuiManager.hh"

#include "ecs/EcsImpl.hh"
#include "graphics/gui/definitions/ConsoleGui.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>

namespace sp {
    DebugGuiManager::DebugGuiManager() : SystemGuiManager("debug") {
        auto lock = ecs::StartTransaction<ecs::AddRemove>();

        auto gui = guiEntity.Get(lock);
        Assert(gui.Has<ecs::EventInput>(lock), "Expected debug gui to start with an EventInput");

        auto &eventInput = gui.Get<ecs::EventInput>(lock);
        eventInput.Register(lock, events, INPUT_EVENT_TOGGLE_CONSOLE);

        guiObserver = lock.Watch<ecs::ComponentEvent<ecs::Gui>>();

        for (auto &ent : lock.EntitiesWith<ecs::Gui>()) {
            guis.emplace_back(GuiEntityContext{ent});
        }
    }

    void DebugGuiManager::DefineWindows() {
        ZoneScoped;
        SetGuiContext();
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

        static ConsoleGui console;
        if (consoleOpen) console.Add();

        for (auto &component : components) {
            auto *window = dynamic_cast<GuiWindow *>(component.get());

            if (window) {
                window->PreDefine();
                ImGui::Begin(component->name.c_str(), nullptr, window->flags);
            }
            component->DefineContents();
            if (window) {
                ImGui::End();
                window->PostDefine();
            }
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

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                if (event.name == INPUT_EVENT_TOGGLE_CONSOLE) {
                    consoleOpen = !consoleOpen;

                    if (lock.Has<ecs::FocusLock>()) {
                        auto &focusLock = lock.Get<ecs::FocusLock>();
                        focusChanged = focusLock.HasFocus(ecs::FocusLayer::Overlay) != consoleOpen;
                    }
                }
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
                if (!ctx.window && !gui.windowName.empty()) {
                    ctx.window = CreateGuiWindow(gui.windowName, ctx.entity);
                }
                if (gui.target == ecs::GuiTarget::Debug) {
                    Attach(ctx.window);
                } else {
                    Detach(ctx.window);
                }
            }
        }
        if (focusChanged) {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::FocusLock>>();
            auto &focusLock = lock.Get<ecs::FocusLock>();
            if (consoleOpen) {
                focusLock.AcquireFocus(ecs::FocusLayer::Overlay);
            } else {
                focusLock.ReleaseFocus(ecs::FocusLayer::Overlay);
            }
        }
    }
} // namespace sp
