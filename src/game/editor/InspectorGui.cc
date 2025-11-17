/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "InspectorGui.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/StructFieldTypes.hh"
#include "editor/EditorControls.hh"
#include "editor/EditorSystem.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace sp {
    InspectorGui::InspectorGui(const string &name)
        : ecs::GuiDefinition(name, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove) {
        context = make_shared<EditorContext>();

        GetSceneManager().QueueAction([inspectorEntity = this->inspectorEntity, events = this->events] {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::EventInput>>();
            ecs::Entity inspector = inspectorEntity.Get(lock);
            if (!inspector.Has<ecs::EventInput>(lock)) return;

            auto &eventInput = inspector.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, EDITOR_EVENT_EDIT_TARGET);
        });
    }

    InspectorGui::~InspectorGui() {
        ecs::QueueTransaction<ecs::Write<ecs::EventInput>>(
            [inspectorEntity = this->inspectorEntity, events = this->events](auto &lock) {
                auto ent = inspectorEntity.Get(lock);
                if (ent.Has<ecs::EventInput>(lock)) {
                    auto &eventInput = ent.Get<ecs::EventInput>(lock);
                    eventInput.Unregister(events, EDITOR_EVENT_EDIT_TARGET);
                }
            });
    }

    bool InspectorGui::PreDefine(ecs::Entity ent) {
        if (!context) return false;
        ZoneScoped;
        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput, ecs::ActiveScene>>();

            if (lock.Has<ecs::ActiveScene>()) {
                auto &active = lock.Get<ecs::ActiveScene>();
                if (context->scene != active.scene) {
                    ecs::QueueTransaction<ecs::Write<ecs::ActiveScene>>([scene = context->scene](auto &lock) {
                        if (lock.template Has<ecs::ActiveScene>()) {
                            lock.template Set<ecs::ActiveScene>(scene);
                        }
                    });
                }
            }

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                if (event.name != EDITOR_EVENT_EDIT_TARGET) continue;

                if (event.data.type == ecs::EventDataType::Entity) {
                    ecs::Entity newTarget = event.data.ent;
                    targetEntity = newTarget;
                    targetStagingEntity = ecs::IsStaging(newTarget);
                } else {
                    Errorf("Invalid editor event: %s", event.ToString());
                }
            }
            if (!targetEntity) return false;
        }

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.01f, 0.01f, 0.01f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.15f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.10f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.10f, 0.10f, 0.35f, 1.0f));
        return true;
    }

    void InspectorGui::PostDefine(ecs::Entity ent) {
        ImGui::PopStyleColor(5);
    }

    void InspectorGui::DefineContents(ecs::Entity ent) {
        ZoneScoped;

        if (ImGui::BeginTabBar("EditMode")) {
            ImGuiTabItemFlags openFlags = ImGuiTabItemFlags_None;
            if (targetEntity && !targetStagingEntity && stagingTabSelected) openFlags = ImGuiTabItemFlags_SetSelected;
            if (ImGui::BeginTabItem("Live View", nullptr, openFlags)) {
                stagingTabSelected = false;
                if (ImGui::Button("Close")) {
                    targetEntity = {};
                } else {
                    auto liveLock = ecs::StartTransaction<ecs::ReadAll>();
                    context->ShowEntityControls(liveLock, targetEntity);
                    if (targetEntity != context->target) {
                        ecs::QueueTransaction<ecs::SendEventsLock>([this](auto &lock) {
                            ecs::EventBindings::SendEvent(lock,
                                inspectorEntity,
                                ecs::Event{EDITOR_EVENT_EDIT_TARGET, inspectorEntity.Get(lock), context->target});
                        });
                    } else {
                        targetStagingEntity = ecs::IsStaging(context->target);
                    }
                }
                ImGui::EndTabItem();
            }
            openFlags = ImGuiTabItemFlags_None;
            if (targetEntity && targetStagingEntity && !stagingTabSelected) openFlags = ImGuiTabItemFlags_SetSelected;
            if (ImGui::BeginTabItem("Entity View", nullptr, openFlags)) {
                stagingTabSelected = true;
                if (ImGui::Button("Close")) {
                    targetEntity = {};
                } else {
                    auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll>();
                    context->ShowEntityControls(stagingLock, targetEntity);
                    auto targetRoot = context->target;
                    if (ecs::IsStaging(targetRoot) && targetRoot.Has<ecs::SceneInfo>(stagingLock)) {
                        auto &stagingInfo = targetRoot.Get<ecs::SceneInfo>(stagingLock);
                        targetRoot = stagingInfo.rootStagingId;
                    }
                    if (targetEntity != targetRoot) {
                        ecs::QueueTransaction<ecs::SendEventsLock>([this](auto &lock) {
                            ecs::EventBindings::SendEvent(lock,
                                inspectorEntity,
                                ecs::Event{EDITOR_EVENT_EDIT_TARGET, inspectorEntity.Get(lock), context->target});
                        });
                    } else {
                        targetStagingEntity = ecs::IsStaging(context->target);
                    }
                }
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
