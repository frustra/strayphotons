#pragma once

#include "InspectorGui.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/StructFieldTypes.hh"
#include "editor/EditorSystem.hh"
#include "graphics/gui/EditorControls.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <picojson/picojson.h>

namespace sp {
    InspectorGui::InspectorGui(const string &name) : GuiWindow(name) {
        std::thread([this]() {
            auto lock = ecs::StartTransaction<ecs::Write<ecs::EventInput>>();

            auto inspector = inspectorEntity.Get(lock);
            if (!inspector.Has<ecs::EventInput>(lock)) return;

            auto &eventInput = inspector.Get<ecs::EventInput>(lock);
            eventInput.Register(lock, events, EDITOR_EVENT_EDIT_TARGET);
        }).detach();
    }

    void InspectorGui::AppendEntity(int depth,
        ecs::Entity ent,
        ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree>> lock) {
        auto name = ecs::ToString(lock, ent);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (children[ent.index].empty()) flags |= ImGuiTreeNodeFlags_Leaf;

        bool open = ImGui::TreeNodeEx((void *)(uintptr_t)ent.index, flags, "%s", name.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            targetEntity = ent;
        }
        if (!open) return;

        for (auto child : children[ent.index]) {
            AppendEntity(depth + 1, child, lock);
        }

        ImGui::TreePop();
    }

    void InspectorGui::ListEntitiesByTransformTree() {
        children.clear();

        auto lock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::TransformTree>>();
        auto entities = lock.EntitiesWith<ecs::TransformTree>();

        children.resize(entities.back().index + 1);

        for (auto &ent : entities) {
            auto &tree = ent.Get<ecs::TransformTree>(lock);
            if (tree.parent) children[tree.parent.Get(lock).index].push_back(ent);
        }

        for (auto &ent : entities) {
            auto &tree = ent.Get<ecs::TransformTree>(lock);
            if (tree.parent) continue;
            AppendEntity(0, ent, lock);
        }
    }

    void InspectorGui::DefineContents() {
        {
            auto stagingLock = ecs::StartStagingTransaction<ecs::ReadAll>();
            auto liveLock = ecs::StartTransaction<ecs::ReadAll>();

            auto inspector = inspectorEntity.Get(liveLock);
            if (!inspector.Has<ecs::EventInput>(liveLock)) return;

            ecs::Event event;
            while (ecs::EventInput::Poll(liveLock, events, event)) {
                if (event.name != EDITOR_EVENT_EDIT_TARGET) continue;

                auto newTarget = std::get_if<ecs::Entity>(&event.data);
                if (!newTarget) {
                    Errorf("Invalid editor event: %s", event.toString());
                } else {
                    targetEntity = *newTarget;
                }
            }

            if (ImGui::Button("Entity Selector")) {
                targetEntity = {};
                return;
            }

            auto inspectTarget = targetEntity.Get(liveLock);
            if (inspectTarget.Exists(liveLock)) {
                ImGui::Text("Entity: %s", ecs::ToString(liveLock, inspectTarget).c_str());

                std::shared_ptr<Scene> liveScene;
                if (inspector.Has<ecs::SceneInfo>(liveLock)) {
                    auto &sceneInfo = inspector.Get<ecs::SceneInfo>(liveLock);
                    liveScene = sceneInfo.scene.lock();
                }

                if (ImGui::BeginTabBar("EditMode", ImGuiTabBarFlags_None)) {
                    if (ImGui::BeginTabItem("Live")) {
                        ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
                            if (!comp.HasComponent(liveLock, inspectTarget)) return;
                            if (ImGui::CollapsingHeader(comp.name, ImGuiTreeNodeFlags_DefaultOpen)) {
                                const void *component = comp.Access(liveLock, inspectTarget);
                                for (auto &field : comp.metadata.fields) {
                                    ecs::GetFieldType(field.type, [&](auto *typePtr) {
                                        using T = std::remove_pointer_t<decltype(typePtr)>;
                                        AddFieldControls<T>(field, comp, liveScene, inspectTarget, component);
                                    });
                                }
                            }
                        });
                        ImGui::EndTabItem();
                    }
                    if (inspectTarget.Has<ecs::SceneInfo>(liveLock)) {
                        auto &sceneInfo = inspectTarget.Get<ecs::SceneInfo>(liveLock);
                        std::vector<ecs::Entity> stagingIds;
                        auto stagingId = sceneInfo.stagingId;
                        while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                            stagingIds.emplace_back(stagingId);

                            auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                            stagingId = stagingInfo.nextStagingId;
                        }
                        while (!stagingIds.empty()) {
                            auto stagingEnt = stagingIds.back();

                            auto &stagingSceneInfo = stagingEnt.Get<ecs::SceneInfo>(stagingLock);
                            auto stagingScene = stagingSceneInfo.scene.lock();
                            if (stagingScene) {
                                auto tabName = "Staging - " + stagingScene->name;
                                if (ImGui::BeginTabItem(tabName.c_str())) {
                                    ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
                                        if (!comp.HasComponent(stagingLock, stagingEnt)) return;
                                        if (ImGui::CollapsingHeader(comp.name, ImGuiTreeNodeFlags_DefaultOpen)) {
                                            const void *component = comp.Access(stagingLock, stagingEnt);
                                            for (auto &field : comp.metadata.fields) {
                                                ecs::GetFieldType(field.type, [&](auto *typePtr) {
                                                    using T = std::remove_pointer_t<decltype(typePtr)>;
                                                    AddFieldControls<T>(field,
                                                        comp,
                                                        stagingScene,
                                                        stagingEnt,
                                                        component);
                                                });
                                            }
                                        }
                                    });
                                    ImGui::EndTabItem();
                                }
                            } else {
                                Logf("Missing staging scene! %s", std::to_string(stagingEnt));
                            }

                            stagingIds.pop_back();
                        }
                    }
                    ImGui::EndTabBar();
                }
            } else if (inspectTarget) {
                ImGui::Text("Missing Entity: %s", ecs::ToString(liveLock, inspectTarget).c_str());
            }
        }
        if (!targetEntity) {
            ListEntitiesByTransformTree();
        }
    }
} // namespace sp
