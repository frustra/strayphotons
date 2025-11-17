/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "OverlayGuiManager.hh"

#include "console/ConsoleGui.hh"
#include "console/FpsCounterGui.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/components/GuiElement.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>

namespace sp {
    OverlayGuiManager::OverlayGuiManager(const ecs::EntityRef &guiEntity) : GuiContext(guiEntity) {
        consoleGui = std::make_shared<ConsoleGui>();
        Attach(consoleGui, ecs::GuiLayoutAnchor::Top, {-1, 300});
        fpsCounterGui = std::make_shared<FpsCounterGui>();
        Attach(fpsCounterGui);

        if (guiEntity) {
            ecs::QueueTransaction<ecs::Write<ecs::EventInput>>(
                [guiEntity = this->guiEntity, events = this->events](auto &lock) {
                    ecs::Entity ent = guiEntity.Get(lock);
                    Assertf(ent.Has<ecs::EventInput>(lock),
                        "Expected overlay gui to start with an EventInput: %s",
                        guiEntity.Name().String());

                    auto &eventInput = ent.Get<ecs::EventInput>(lock);
                    eventInput.Register(lock, events, INPUT_EVENT_TOGGLE_CONSOLE);
                });
        }
    }

    OverlayGuiManager::~OverlayGuiManager() {
        if (guiEntity) {
            ecs::QueueTransaction<ecs::Write<ecs::EventInput>>(
                [guiEntity = this->guiEntity, events = this->events](auto &lock) {
                    auto ent = guiEntity.Get(lock);
                    if (ent.Has<ecs::EventInput>(lock)) {
                        auto &eventInput = ent.Get<ecs::EventInput>(lock);
                        eventInput.Unregister(events, INPUT_EVENT_TOGGLE_CONSOLE);
                    }
                });
        }
    }

    std::shared_ptr<GuiContext> OverlayGuiManager::CreateContext(const ecs::Name &guiName) {
        GetSceneManager().QueueActionAndBlock(SceneAction::ApplySystemScene,
            "gui",
            [name = guiName](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                auto ent = scene->NewSystemEntity(lock, scene, name);
                ent.Set<ecs::EventInput>(lock);
                ent.Set<ecs::RenderOutput>(lock);
            });

        ecs::EntityRef ref(guiName);
        auto guiContext = std::shared_ptr<OverlayGuiManager>(new OverlayGuiManager(ref));
        {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::RenderOutput>>();

            auto ent = ref.Get(lock);
            Assert(ent.Has<ecs::RenderOutput>(lock), "Expected overlay gui to start with a RenderOutput");

            ent.Get<ecs::RenderOutput>(lock).guiContext = guiContext;
        }
        return guiContext;
    }

    void OverlayGuiManager::DefineWindows() {
        ZoneScoped;
        SetGuiContext();
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.0f, 0.0f, 0.0f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

        GuiContext::DefineWindows();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    bool OverlayGuiManager::BeforeFrame() {
        GuiContext::BeforeFrame();

        bool focusChanged = false;
        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput, ecs::FocusLock>>();

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
        return true;
    }
} // namespace sp
