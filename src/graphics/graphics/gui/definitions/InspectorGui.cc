/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "InspectorGui.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/StructFieldTypes.hh"
#include "editor/EditorSystem.hh"
#include "graphics/gui/definitions/EditorControls.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace sp {
    InspectorGui::InspectorGui(const string &name)
        : GuiWindow(name, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove) {
        context = make_shared<EditorContext>();

        ecs::QueueTransaction<ecs::Write<ecs::EventInput>>([this](auto &lock) {
            ecs::Entity inspector = inspectorEntity.Get(lock);
            if (!inspector.Has<ecs::EventInput>(lock)) return;

            auto &eventInput = inspector.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, EDITOR_EVENT_EDIT_TARGET);
        });
    }

    void InspectorGui::PreDefine() {
        auto *viewport = ImGui::GetMainViewport();
        ImGuiIO &io = ImGui::GetIO();
        ImGui::SetNextWindowSize(ImVec2(std::min(500.0f, io.DisplaySize.x), viewport->Size.y));
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y),
            ImGuiCond_None,
            ImVec2(1, 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.01f, 0.01f, 0.01f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.15f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.10f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.10f, 0.10f, 0.35f, 1.0f));
    }

    void InspectorGui::PostDefine() {
        ImGui::PopStyleColor(5);
    }

    void InspectorGui::DefineContents() {
        if (!context) return;
        ZoneScoped;

        bool selectEntityView = false;
        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput, ecs::ActiveScene>>();

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                if (event.name != EDITOR_EVENT_EDIT_TARGET) continue;

                if (std::holds_alternative<ecs::Entity>(event.data)) {
                    auto newTarget = std::get<ecs::Entity>(event.data);
                    targetEntity = newTarget;
                    if (newTarget && ecs::IsStaging(newTarget)) selectEntityView = true;
                } else {
                    Errorf("Invalid editor event: %s", event.ToString());
                }
            }

            if (lock.Has<ecs::ActiveScene>()) {
                auto &active = lock.Get<ecs::ActiveScene>();
                if (context->scene != active.scene) {
                    std::thread([scene = context->scene]() {
                        auto lock = ecs::StartTransaction<ecs::Write<ecs::ActiveScene>>();
                        if (lock.Has<ecs::ActiveScene>()) {
                            lock.Set<ecs::ActiveScene>(scene);
                        }
                    }).detach();
                }
            }
        }

        if (ImGui::BeginTabBar("EditMode")) {
            bool liveTabOpen = ImGui::BeginTabItem("Live View");
            if (!targetEntity && ImGui::IsItemActivated()) {
                context->RefreshEntityTree();
            }
            if (liveTabOpen) {
                if (targetEntity) {
                    if (ImGui::Button("Show Entity Tree")) {
                        context->RefreshEntityTree();
                        targetEntity = {};
                    }

                    auto liveLock = ecs::StartTransaction<ecs::ReadAll>();
                    context->ShowEntityControls(liveLock, targetEntity);
                } else {
                    targetEntity = context->ShowEntityTree();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Entity View",
                    nullptr,
                    selectEntityView ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
                if (!targetEntity) {
                    targetEntity = context->ShowAllEntities("##EntityList");
                } else {
                    if (ImGui::Button("Show All Entities")) {
                        targetEntity = {};
                    } else {
                        auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll>();

                        context->ShowEntityControls(stagingLock, targetEntity);
                        targetEntity = context->target;
                    }
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Scene View")) {
                auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll>();
                context->ShowSceneControls(stagingLock);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Signal Debugger")) {
                auto liveLock = ecs::StartTransaction<ecs::ReadAll>();
                context->ShowSignalControls(liveLock);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
} // namespace sp
