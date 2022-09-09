#pragma once

#include "ecs/EcsImpl.hh"
#include "editor/EditorSystem.hh"
#include "graphics/gui/EditorControls.hh"
#include "graphics/gui/GuiContext.hh"
#include "input/BindingNames.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <picojson/picojson.h>

namespace sp {
    class InspectorGui : public GuiWindow {
    public:
        InspectorGui(const string &name) : GuiWindow(name) {}
        virtual ~InspectorGui() {}

        void DefineContents() {
            {
                auto lock = ecs::World.StartTransaction<ecs::ReadAll>();

                auto inspector = inspectorEntity.Get(lock);
                if (!inspector.Has<ecs::EventInput>(lock)) return;

                ecs::Event event;
                while (ecs::EventInput::Poll(lock, inspector, EDITOR_EVENT_EDIT_TARGET, event)) {
                    auto newTarget = std::get_if<ecs::Entity>(&event.data);
                    if (!newTarget) {
                        Errorf("Invalid editor event: %s", event.toString());
                    } else {
                        targetEntity = *newTarget;
                    }
                }

                if (ImGui::Button("List Entities")) {
                    targetEntity = {};
                    return;
                }

                auto inspectTarget = targetEntity.Get(lock);
                if (inspectTarget.Exists(lock)) {
                    ImGui::Text("Entity: %s", ecs::ToString(lock, inspectTarget).c_str());

                    ecs::EntityScope scope;
                    if (inspector.Has<ecs::SceneInfo>(lock)) {
                        auto &sceneInfo = inspector.Get<ecs::SceneInfo>(lock);
                        scope.scene = sceneInfo.scene;
                        auto scene = sceneInfo.scene.lock();
                        if (scene) scope.prefix.scene = scene->name;
                    }

                    ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
                        if (!comp.HasComponent(lock, inspectTarget)) return;
                        if (ImGui::CollapsingHeader(comp.name, ImGuiTreeNodeFlags_DefaultOpen)) {
                            const void *component = comp.Access(lock, inspectTarget);
                            for (auto &field : comp.fields) {
                                ecs::GetFieldType(field.type, [&](auto *typePtr) {
                                    using T = std::remove_pointer_t<decltype(typePtr)>;
                                    AddFieldControls<T>(field, comp, inspectTarget, component);
                                });
                            }
                        }
                    });
                } else if (inspectTarget) {
                    ImGui::Text("Missing Entity: %s", ecs::ToString(lock, inspectTarget).c_str());
                }
            }
            if (!targetEntity) {
                ListEntitiesByTransformTree();
            }
        }

    private:
        void ListEntitiesByTransformTree() {
            children.clear();

            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name, ecs::TransformTree>>();
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

        void AppendEntity(int depth, ecs::Entity ent, ecs::Lock<ecs::Read<ecs::Name, ecs::TransformTree>> lock) {
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

    private:
        vector<vector<ecs::Entity>> children;

        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");
        ecs::EntityRef targetEntity;
    };
} // namespace sp
