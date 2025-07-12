/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "OverlayGuiManager.hh"

#include "ecs/EcsImpl.hh"
#include "graphics/gui/definitions/ConsoleGui.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>

namespace sp {
    OverlayGuiManager::OverlayGuiManager() : FlatViewGuiContext("debug") {
        consoleGui = std::make_shared<ConsoleGui>();
        Attach(consoleGui);

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

    void OverlayGuiManager::DefineWindows() {
        ZoneScoped;
        SetGuiContext();
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

        std::sort(components.begin(), components.end(), [](const Ref &lhs, const Ref &rhs) {
            if (!lhs || !rhs) return lhs < rhs;
            return lhs->anchor < rhs->anchor;
        });

        ImGuiViewport *imguiViewport = ImGui::GetMainViewport();
        Assertf(imguiViewport, "ImGui::GetMainViewport() returned null");
        ImVec2 viewportPos = imguiViewport->WorkPos;
        ImVec2 viewportSize = imguiViewport->WorkSize;
        for (auto &component : components) {
            if (!component) continue;
            GuiRenderable &renderable = *component;

            if (renderable.PreDefine()) {
                ImVec2 windowSize(renderable.preferredSize.x, renderable.preferredSize.y);
                windowSize.x = std::min(windowSize.x, std::min(viewportSize.x, imguiViewport->WorkSize.x * 0.4f));
                windowSize.y = std::min(windowSize.y, std::min(viewportSize.y, imguiViewport->WorkSize.y * 0.4f));
                if (windowSize.x <= 0) windowSize.x = viewportSize.x;
                if (windowSize.y <= 0) windowSize.y = viewportSize.y;
                ImGui::SetNextWindowSize(windowSize);

                switch (renderable.anchor) {
                case GuiLayoutAnchor::Fullscreen:
                    ImGui::SetNextWindowPos(viewportPos);
                    break;
                case GuiLayoutAnchor::Left:
                    ImGui::SetNextWindowPos(viewportPos);
                    viewportPos.x += windowSize.x;
                    viewportSize.x -= windowSize.x;
                    break;
                case GuiLayoutAnchor::Top:
                    ImGui::SetNextWindowPos(viewportPos);
                    viewportPos.y += windowSize.y;
                    viewportSize.y -= windowSize.y;
                    break;
                case GuiLayoutAnchor::Right:
                    ImGui::SetNextWindowPos(ImVec2(viewportPos.x + viewportSize.x, viewportPos.y),
                        ImGuiCond_None,
                        ImVec2(1, 0));
                    viewportSize.x -= windowSize.x;
                    break;
                case GuiLayoutAnchor::Bottom:
                    ImGui::SetNextWindowPos(ImVec2(viewportPos.x, viewportPos.y + viewportSize.y),
                        ImGuiCond_None,
                        ImVec2(0, 1));
                    viewportSize.y -= windowSize.y;
                    break;
                case GuiLayoutAnchor::Floating:
                    // Noop
                    break;
                default:
                    Abortf("Unexpected GuiLayoutAnchor: %s", renderable.anchor);
                }

                ImGui::Begin(component->name.c_str(), nullptr, renderable.windowFlags);
                component->DefineContents();
                ImGui::End();
                renderable.PostDefine();
            }
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    void OverlayGuiManager::BeforeFrame() {
        FlatViewGuiContext::BeforeFrame();

        ImGui::StyleColorsClassic();

        ImGuiIO &io = ImGui::GetIO();
        io.MouseDrawCursor = false;

        bool focusChanged = false;
        {
            auto lock = ecs::StartTransaction<ecs::ReadSignalsLock, ecs::Read<ecs::EventInput, ecs::Gui>>();

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                if (event.name == INPUT_EVENT_TOGGLE_CONSOLE) {
                    consoleGui->consoleOpen = !consoleGui->consoleOpen;

                    if (lock.Has<ecs::FocusLock>()) {
                        auto &focusLock = lock.Get<ecs::FocusLock>();
                        focusChanged = focusLock.HasFocus(ecs::FocusLayer::Overlay) != consoleGui->consoleOpen;
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
                if (gui.target == ecs::GuiTarget::Overlay) {
                    Attach(ctx.window);
                } else {
                    Detach(ctx.window);
                }
            }
        }
        if (focusChanged) {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::FocusLock>>();
            auto &focusLock = lock.Get<ecs::FocusLock>();
            if (consoleGui->consoleOpen) {
                focusLock.AcquireFocus(ecs::FocusLayer::Overlay);
            } else {
                focusLock.ReleaseFocus(ecs::FocusLayer::Overlay);
            }
        }
    }
} // namespace sp
