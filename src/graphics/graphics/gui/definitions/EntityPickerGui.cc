/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EntityPickerGui.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/StructFieldTypes.hh"
#include "editor/EditorSystem.hh"
#include "game/SceneManager.hh"
#include "graphics/gui/definitions/EditorControls.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

namespace sp {
    EntityPickerGui::EntityPickerGui(const string &name)
        : ecs::GuiRenderable(name,
              ecs::GuiLayoutAnchor::Left,
              {400, -1},
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove) {
        context = make_shared<EditorContext>();

        ecs::QueueTransaction<ecs::Write<ecs::EventInput>>([this](auto &lock) {
            ecs::Entity picker = pickerEntity.Get(lock);
            if (!picker.Has<ecs::EventInput>(lock)) return;

            auto &eventInput = picker.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, EDITOR_EVENT_EDIT_TARGET);
        });
    }

    bool EntityPickerGui::PreDefine(ecs::Entity ent) {
        if (!context) return false;
        ZoneScoped;
        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::EventInput>>();

            ecs::Event event;
            while (ecs::EventInput::Poll(lock, events, event)) {
                if (event.name != EDITOR_EVENT_EDIT_TARGET) continue;

                if (event.data.type == ecs::EventDataType::Entity) {
                    targetEntity = event.data.ent;
                } else {
                    Errorf("Invalid editor event: %s", event.ToString());
                }
            }
        }

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.01f, 0.01f, 0.01f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.15f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.10f, 0.10f, 0.35f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.10f, 0.10f, 0.35f, 1.0f));
        return true;
    }

    void EntityPickerGui::PostDefine(ecs::Entity ent) {
        ImGui::PopStyleColor(5);
    }

    void EntityPickerGui::DefineContents(ecs::Entity ent) {
        ZoneScoped;

        if (ImGui::BeginTabBar("EditMode")) {
            const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
            if (ImGui::BeginTabItem("Live View")) {
                if (ImGui::IsItemActivated()) context->RefreshEntityTree();
                ImGui::BeginChild("entityViewScroll", ImVec2(0, -footerHeight));
                if (context->ShowEntityTree(targetEntity)) {
                    ecs::QueueTransaction<ecs::SendEventsLock>([this](auto &lock) {
                        ecs::EventBindings::SendEvent(lock,
                            inspectorEntity,
                            ecs::Event{EDITOR_EVENT_EDIT_TARGET, inspectorEntity.Get(lock), targetEntity.GetLive()});
                    });
                }
                ImGui::EndChild();
                if (ImGui::Button("New Entity") && context->scene) {
                    GetSceneManager().QueueAction(SceneAction::ApplySystemScene,
                        context->scene.data->name,
                        [this](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                            auto newEntity = scene->NewRootEntity(lock, scene);

                            newEntity.Set<ecs::TransformTree>(lock);
                            newEntity.Set<ecs::TransformSnapshot>(lock);

                            GetSceneManager().QueueAction([this, newTarget = ecs::EntityRef(newEntity)] {
                                ecs::QueueTransaction<ecs::SendEventsLock>([this, newTarget](auto &lock) {
                                    ecs::EventBindings::SendEvent(lock,
                                        inspectorEntity,
                                        ecs::Event{EDITOR_EVENT_EDIT_TARGET,
                                            pickerEntity.GetLive(),
                                            newTarget.GetLive()});
                                });
                            });
                        });
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Entity View")) {
                ImGui::BeginChild("entityViewScroll", ImVec2(0, -footerHeight));
                if (context->ShowAllEntities(targetEntity, "##EntityList")) {
                    ecs::QueueTransaction<ecs::SendEventsLock>([this](auto &lock) {
                        ecs::EventBindings::SendEvent(lock,
                            inspectorEntity,
                            ecs::Event{EDITOR_EVENT_EDIT_TARGET, inspectorEntity.Get(lock), targetEntity.GetStaging()});
                    });
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Scene View")) {
                auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll>();
                context->ShowSceneControls(stagingLock);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
} // namespace sp
