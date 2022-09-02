#pragma once

#include "ecs/EcsImpl.hh"
#include "editor/EditorSystem.hh"
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
                        inspectTarget = *newTarget;
                    }
                }

                if (inspectTarget) {
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
                        ImGui::Text("%s:", comp.name);
                        const void *component = comp.Access(lock, inspectTarget);
                        for (auto &field : comp.fields) {
                            if (field.type == ecs::FieldType::Bool) {
                                auto value = *field.Access<bool>(component);
                                if (ImGui::Checkbox(field.name, &value)) {
                                    GEditor.PushEdit([this, value, &comp, &field]() {
                                        auto lock = ecs::World.StartTransaction<ecs::WriteAll>();
                                        void *component = comp.Access(lock, inspectTarget);
                                        *field.Access<bool>(component) = value;
                                        Logf("Changed %s.%s to %u", comp.name, field.name, value);
                                    });
                                }
                            } else if (field.type == ecs::FieldType::Int32) {
                                auto value = *field.Access<int32_t>(component);
                                if (ImGui::InputScalarN(field.name, ImGuiDataType_S32, &value, 1, NULL, NULL, "%d")) {
                                    Logf("Changed %s.%s to %d", comp.name, field.name, value);
                                }
                            } else if (field.type == ecs::FieldType::Uint32) {
                                auto value = *field.Access<uint32_t>(component);
                                if (ImGui::InputScalarN(field.name, ImGuiDataType_U32, &value, 1, NULL, NULL, "%u")) {
                                    Logf("Changed %s.%s to %u", comp.name, field.name, value);
                                }
                            } else if (field.type == ecs::FieldType::SizeT) {
                                auto value = *field.Access<size_t>(component);
                                if (ImGui::InputScalarN(field.name, ImGuiDataType_U64, &value, 1, NULL, NULL, "%u")) {
                                    Logf("Changed %s.%s to %u", comp.name, field.name, value);
                                }
                                // } else if (field.type == ecs::FieldType::AngleT) {
                                //     auto value = *field.Access<sp::angle_t>(component);
                            } else if (field.type == ecs::FieldType::Float) {
                                auto value = *field.Access<float>(component);
                                if (ImGui::InputFloat(field.name, &value)) {
                                    Logf("Changed %s.%s to %f", comp.name, field.name, value);
                                }
                            } else if (field.type == ecs::FieldType::Vec2) {
                                auto value = *field.Access<glm::vec2>(component);
                                if (ImGui::InputFloat2(field.name, (float *)&value)) {
                                    Logf("Changed %s.%s to %s", comp.name, field.name, glm::to_string(value));
                                }
                            } else if (field.type == ecs::FieldType::Vec3) {
                                auto value = *field.Access<glm::vec3>(component);
                                if (ImGui::InputFloat3(field.name, (float *)&value)) {
                                    Logf("Changed %s.%s to %s", comp.name, field.name, glm::to_string(value));
                                }
                            } else if (field.type == ecs::FieldType::Vec4) {
                                auto value = *field.Access<glm::vec4>(component);
                                if (ImGui::InputFloat4(field.name, (float *)&value)) {
                                    Logf("Changed %s.%s to %s", comp.name, field.name, glm::to_string(value));
                                }
                            } else if (field.type == ecs::FieldType::IVec2) {
                                auto value = *field.Access<glm::ivec2>(component);
                                if (ImGui::InputInt2(field.name, (int *)&value)) {
                                    Logf("Changed %s.%s to %s", comp.name, field.name, glm::to_string(value));
                                }
                            } else if (field.type == ecs::FieldType::IVec3) {
                                auto value = *field.Access<glm::ivec3>(component);
                                if (ImGui::InputInt3(field.name, (int *)&value)) {
                                    Logf("Changed %s.%s to %s", comp.name, field.name, glm::to_string(value));
                                }
                                // } else if (field.type == ecs::FieldType::Quat) {
                                //     auto value = *field.Access<glm::quat>(component);
                            } else if (field.type == ecs::FieldType::String) {
                                auto value = *field.Access<std::string>(component);
                                if (ImGui::InputText(field.name, &value)) {
                                    Logf("Changed %s.%s to %s", comp.name, field.name, value);
                                }
                                // } else if (field.type == ecs::FieldType::EntityRef) {
                                //     auto value = *field.Access<ecs::EntityRef>(component);
                                // } else if (field.type == ecs::FieldType::Transform) {
                                //     auto value = *field.Access<ecs::Transform>(component);
                                // } else if (field.type == ecs::FieldType::AnimationStates) {
                                //     auto value = *field.Access<std::vector<ecs::AnimationState>>(component);
                                // } else if (field.type == ecs::FieldType::InterpolationMode) {
                                //     auto value = *field.Access<ecs::InterpolationMode>(component);
                                // } else if (field.type == ecs::FieldType::VisibilityMask) {
                                //     auto value = *field.Access<ecs::VisibilityMask>(component);
                            }
                        }
                    });
                }
            }
            if (!inspectTarget) {
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
            if (!open) return;

            for (auto child : children[ent.index]) {
                AppendEntity(depth + 1, child, lock);
            }

            ImGui::TreePop();
        }

    private:
        vector<vector<ecs::Entity>> children;

        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");
        ecs::Entity inspectTarget;
    };
} // namespace sp
