#pragma once

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "core/Defer.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/SignalExpression.hh"
#include "game/SceneManager.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <magic_enum.hpp>
#include <map>

namespace sp {
    struct EditorContext {
        // Persistent context
        struct TreeNode {
            bool hasParent = false;
            std::vector<ecs::EntityRef> children;
        };
        std::map<ecs::EntityRef, TreeNode> entityTree;
        ecs::EventQueueRef events = ecs::NewEventQueue();
        ecs::EntityRef inspectorEntity = ecs::Name("editor", "inspector");
        ecs::EntityRef targetEntity;
        std::string entitySearch;

        // Temporary context
        ecs::Lock<ecs::ReadAll> *liveLock = nullptr;
        ecs::Lock<ecs::ReadAll> *stagingLock = nullptr;
        std::shared_ptr<Scene> scene;
        ecs::Entity target;
        std::string fieldName, fieldId;

        void RefreshEntityTree() {
            entityTree.clear();

            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::TransformTree>>();
            for (auto &ent : lock.EntitiesWith<ecs::TransformTree>()) {
                auto &tree = ent.Get<ecs::TransformTree>(lock);
                entityTree[ent].hasParent = (bool)tree.parent;
                if (tree.parent) {
                    entityTree[tree.parent].children.emplace_back(ent);
                }
            }
        }

        void ShowEntityTree(ecs::EntityRef root = ecs::EntityRef()) {
            if (!root) {
                if (ImGui::Button("Refresh List") || entityTree.empty()) {
                    RefreshEntityTree();
                }
                for (auto &pair : entityTree) {
                    if (!pair.second.hasParent) ShowEntityTree(pair.first);
                }
            } else {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
                if (entityTree[root].children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

                bool open = ImGui::TreeNodeEx(root.Name().String().c_str(), flags);
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    targetEntity = root;
                }
                if (!open) return;

                for (auto &child : entityTree[root].children) {
                    if (child) ShowEntityTree(child);
                }

                ImGui::TreePop();
            }
        }

        void ShowEntityEditControls(ecs::Lock<ecs::ReadAll> liveLock, ecs::Lock<ecs::ReadAll> stagingLock);

        template<typename T>
        bool AddImGuiElement(const std::string &name, T &value);
        template<typename T>
        void AddFieldControls(const ecs::StructField &field, const ecs::ComponentBase &comp, const void *component);
    };

    template<typename T>
    bool EditorContext::AddImGuiElement(const std::string &name, T &value) {
        bool changed = false;
        if constexpr (std::is_enum_v<T>) {
            auto items = magic_enum::enum_entries<T>();
            if constexpr (magic_enum::detail::is_flags_v<T>) {
                if (ImGui::BeginListBox(name.c_str(), ImVec2(0, 5.25 * ImGui::GetTextLineHeightWithSpacing()))) {
                    for (auto &item : items) {
                        if (item.second.empty()) continue;
                        const bool is_selected = (value & item.first) == item.first;
                        if (ImGui::Selectable(item.second.data(), is_selected)) {
                            value ^= item.first;
                            changed = true;
                        }
                    }
                    ImGui::EndListBox();
                }
            } else {
                std::string enumName(magic_enum::enum_name(value));
                if (ImGui::BeginCombo(name.c_str(), enumName.c_str())) {
                    for (auto &item : items) {
                        if (item.second.empty()) continue;
                        const bool is_selected = item.first == value;
                        if (ImGui::Selectable(item.second.data(), is_selected)) {
                            value = item.first;
                            changed = true;
                        }

                        if (is_selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        } else {
            picojson::value jsonValue;
            json::Save({}, jsonValue, value);
            if (fieldName.empty()) {
                ImGui::Text("%s", jsonValue.serialize(true).c_str());
            } else {
                ImGui::Text("%s: %s", fieldName.c_str(), jsonValue.serialize(true).c_str());
            }
        }
        return changed;
    }

    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, bool &value) {
        return ImGui::Checkbox(name.c_str(), &value);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, int32_t &value) {
        return ImGui::DragScalar(name.c_str(), ImGuiDataType_S32, &value, 1.0f, NULL, NULL, "%d");
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, uint32_t &value) {
        return ImGui::DragScalar(name.c_str(), ImGuiDataType_U32, &value, 1.0f, NULL, NULL, "%u");
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, size_t &value) {
        return ImGui::DragScalar(name.c_str(), ImGuiDataType_U64, &value, 1.0f, NULL, NULL, "%u");
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, sp::angle_t &value) {
        return ImGui::SliderAngle(name.c_str(), &value.radians(), 0.0f, 360.0f);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, float &value) {
        return ImGui::DragFloat(name.c_str(), &value, 0.01f);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, glm::vec2 &value) {
        return ImGui::DragFloat2(name.c_str(), (float *)&value, 0.01f);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, glm::vec3 &value) {
        return ImGui::DragFloat3(name.c_str(), (float *)&value, 0.01f);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, glm::vec4 &value) {
        return ImGui::DragFloat4(name.c_str(), (float *)&value, 0.01f);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, color_t &value) {
        return ImGui::ColorEdit3(name.c_str(), (float *)&value);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, color_alpha_t &value) {
        return ImGui::ColorEdit4(name.c_str(), (float *)&value);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, glm::ivec2 &value) {
        return ImGui::DragInt2(name.c_str(), (int *)&value);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, glm::ivec3 &value) {
        return ImGui::DragInt3(name.c_str(), (int *)&value);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, glm::quat &value) {
        // TODO: Add grab handle for orientation
        glm::vec3 angles = glm::degrees(glm::eulerAngles(value));
        for (glm::length_t i = 0; i < angles.length(); i++) {
            if (angles[i] < 0.0f) angles[i] += 360.0f;
        }

        if (ImGui::SliderFloat3(name.c_str(), (float *)&angles, 0.0f, 360.0f, "%.1f deg")) {
            value = glm::quat(glm::radians(angles));
            return true;
        }
        return false;
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, std::string &value) {
        return ImGui::InputText(name.c_str(), &value);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, ecs::SignalExpression &value) {
        // TODO: figure out how to re-parse the expression
        return ImGui::InputText(name.c_str(), &value.expr);
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, ecs::EntityRef &value) {
        bool changed = false;
        if (!fieldName.empty()) {
            ImGui::Text("%s:", fieldName.c_str());
            ImGui::SameLine();
        }
        ImGui::Button(value ? value.Name().String().c_str() : "None");
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##entity_search", "Entity Search", &entitySearch);
            if (ImGui::BeginListBox(fieldId.c_str(), ImVec2(400, ImGui::GetTextLineHeightWithSpacing() * 25))) {
                auto entityNames = ecs::GetEntityRefs().GetNames(entitySearch);
                for (auto &entName : entityNames) {
                    if (ImGui::Selectable(entName.String().c_str())) {
                        value = entName;
                        changed = true;
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndListBox();
            }
            ImGui::EndPopup();
        }
        return changed;
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, ecs::Transform &value) {
        // TODO: Add grab handle in view

        auto text = "position" + fieldId;
        bool changed = ImGui::DragFloat3(text.c_str(), (float *)&value.matrix[3], 0.01f);

        text = "rotation" + fieldId;
        glm::vec3 angles = glm::degrees(glm::eulerAngles(value.GetRotation()));
        for (glm::length_t i = 0; i < angles.length(); i++) {
            if (angles[i] < 0.0f) angles[i] += 360.0f;
            if (angles[i] >= 360.0f) angles[i] -= 360.0f;
        }
        if (ImGui::SliderFloat3(text.c_str(), (float *)&angles, 0.0f, 360.0f, "%.1f deg")) {
            value.SetRotation(glm::quat(glm::radians(angles)));
            changed = true;
        }

        text = "scale" + fieldId;
        glm::vec3 scale = value.GetScale();
        if (ImGui::DragFloat3(text.c_str(), (float *)&scale, 0.01f)) {
            value.SetScale(scale);
            changed = true;
        }
        return changed;
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, std::vector<ecs::AnimationState> &value) {
        for (auto &state : value) {
            picojson::value jsonValue;
            json::Save({}, jsonValue, state);
            ImGui::Text("%s", jsonValue.serialize(true).c_str());
        }
        return false;
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, std::vector<ecs::ScriptState> &value) {
        bool changed = false;
        std::vector<size_t> removeList;
        for (auto &state : value) {
            std::string rowId = fieldId + "." + std::to_string(state.GetInstanceId());
            bool isOnTick = std::holds_alternative<ecs::OnTickFunc>(state.definition.callback) ||
                            std::holds_alternative<ecs::OnPhysicsUpdateFunc>(state.definition.callback);
            bool isPrefab = std::holds_alternative<ecs::PrefabFunc>(state.definition.callback);
            std::string scriptLabel;
            if (isOnTick) {
                if (state.definition.filterOnEvent) {
                    scriptLabel = "OnEvent: " + state.definition.name;
                } else {
                    scriptLabel = "OnTick: " + state.definition.name;
                }
            } else if (isPrefab) {
                if (state.definition.name == "template") {
                    scriptLabel = "Template: " + state.GetParam<std::string>("source");
                } else if (state.definition.name == "gltf") {
                    scriptLabel = "Gltf: " + state.GetParam<std::string>("model");
                } else {
                    scriptLabel = "Prefab: " + state.definition.name;
                }
            }
            if (ImGui::TreeNodeEx(rowId.c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s", scriptLabel.c_str())) {
                if (ecs::IsStaging(target)) {
                    if (ImGui::Button("-", ImVec2(20, 0))) {
                        removeList.emplace_back(state.GetInstanceId());
                    }
                    ImGui::SameLine();
                }
                if (isOnTick) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::BeginCombo(rowId.c_str(), state.definition.name.c_str())) {
                        auto &scripts = ecs::GetScriptDefinitions().scripts;
                        for (auto &[scriptName, definition] : scripts) {
                            const bool isSelected = state.definition.name == scriptName;
                            if (ImGui::Selectable(scriptName.c_str(), isSelected)) {
                                state.definition = definition;
                                changed = true;
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                } else if (isPrefab) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::BeginCombo(rowId.c_str(), state.definition.name.c_str())) {
                        auto &prefabs = ecs::GetScriptDefinitions().prefabs;
                        for (auto &[prefabName, definition] : prefabs) {
                            const bool isSelected = state.definition.name == prefabName;
                            if (ImGui::Selectable(prefabName.c_str(), isSelected)) {
                                state.definition = definition;
                                changed = true;
                            }
                            if (isSelected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "NULL Script");
                }

                if (state.definition.context) {
                    void *dataPtr = state.definition.context->Access(state);
                    Assertf(dataPtr, "Script definition returned null data: %s", state.definition.name);
                    auto &fields = state.definition.context->metadata.fields;
                    if (!fields.empty()) {
                        ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                                ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
                        if (ImGui::BeginTable(rowId.c_str(), 2, flags)) {
                            ImGui::TableSetupColumn("Parameter");
                            ImGui::TableSetupColumn("Value");
                            ImGui::TableHeadersRow();

                            for (auto &field : fields) {
                                if (field.name) {
                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0);
                                    ImGui::Text("%s", field.name);
                                    ImGui::TableSetColumnIndex(1);
                                    ecs::GetFieldType(field.type, [&](auto *typePtr) {
                                        using T = std::remove_pointer_t<decltype(typePtr)>;

                                        auto fieldValue = field.Access<T>(dataPtr);
                                        auto parentFieldName = fieldName;
                                        fieldName = "";
                                        if (AddImGuiElement(rowId + "."s + field.name, *fieldValue)) {
                                            changed = true;
                                        }
                                        fieldName = parentFieldName;
                                    });
                                }
                            }
                        }
                        ImGui::EndTable();
                    }
                }

                ImGui::TreePop();
            }
        }
        if (ecs::IsStaging(target)) {
            for (auto &instanceId : removeList) {
                sp::erase_if(value, [&](auto &&state) {
                    return state.GetInstanceId() == instanceId;
                });
                changed = true;
            }
            if (ImGui::Button("Add Prefab")) {
                auto &state = value.emplace_back();
                state.scope.scene = scene;
                state.scope.prefix.scene = scene->name;
                state.definition.callback = ecs::PrefabFunc();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Script")) {
                auto &state = value.emplace_back();
                state.scope.scene = scene;
                state.scope.prefix.scene = scene->name;
                state.definition.callback = ecs::OnTickFunc();
                changed = true;
            }
        }
        return changed;
    }

    template<typename T>
    void EditorContext::AddFieldControls(const ecs::StructField &field,
        const ecs::ComponentBase &comp,
        const void *component) {
        auto value = *field.Access<T>(component);
        fieldName = field.name ? field.name : comp.name;
        fieldId = "##"s + comp.name + std::to_string(field.fieldIndex);
        std::string elementName = fieldName + fieldId;

        bool valueChanged = false;
        bool isDefined = true;
        if (ecs::IsStaging(target)) {
            auto defaultLiveComponent = comp.GetLiveDefault();
            auto defaultStagingComponent = comp.GetStagingDefault();
            auto &defaultValue = *field.Access<T>(defaultLiveComponent);
            auto &undefinedValue = *field.Access<T>(defaultStagingComponent);
            if (defaultValue != undefinedValue) {
                isDefined = value != undefinedValue;
                if (ImGui::Checkbox(isDefined ? fieldId.c_str() : elementName.c_str(), &isDefined)) {
                    // Value has already been toggled by ImGui at this point
                    if (!isDefined) {
                        value = undefinedValue;
                    } else {
                        value = defaultValue;
                    }
                    valueChanged = true;
                }
                if (isDefined) ImGui::SameLine();
            }
        }
        if (isDefined) {
            if (AddImGuiElement(elementName, value)) valueChanged = true;
        }

        if (valueChanged) {
            if (ecs::IsLive(target)) {
                GetSceneManager().QueueAction(SceneAction::EditLiveECS,
                    [target = this->target, value, &comp, &field](ecs::Lock<ecs::WriteAll> lock) {
                        void *component = comp.Access(lock, target);
                        *field.Access<T>(component) = value;
                    });
            } else if (scene != nullptr) {
                GetSceneManager().QueueAction(SceneAction::EditStagingScene,
                    scene->name,
                    [target = this->target, value, &comp, &field](ecs::Lock<ecs::AddRemove> lock,
                        std::shared_ptr<Scene> scene) {
                        void *component = comp.Access((ecs::Lock<ecs::WriteAll>)lock, target);
                        *field.Access<T>(component) = value;
                    });
            } else {
                Errorf("Can't add ImGui field controls for null scene: %s", std::to_string(target));
            }
        }
    }

    void EditorContext::ShowEntityEditControls(ecs::Lock<ecs::ReadAll> liveLock, ecs::Lock<ecs::ReadAll> stagingLock) {
        DebugAssert(ecs::IsLive(liveLock), "Expected live lock to point to correct ECS instance");
        DebugAssert(ecs::IsStaging(stagingLock), "Expected staging lock to point to correct ECS instance");

        if (!targetEntity) return;

        this->liveLock = &liveLock;
        this->stagingLock = &stagingLock;
        Defer defer([this] {
            // Clear temporary context
            this->liveLock = nullptr;
            this->stagingLock = nullptr;
            this->scene = nullptr;
            this->target = {};
            this->fieldName = "";
            this->fieldId = "";
        });

        auto inspectTarget = targetEntity.Get(liveLock);
        if (inspectTarget.Has<ecs::SceneInfo>(liveLock)) {
            ImGui::Text("Entity: %s", ecs::ToString(liveLock, inspectTarget).c_str());

            auto &sceneInfo = inspectTarget.Get<ecs::SceneInfo>(liveLock);
            auto liveScene = sceneInfo.scene.lock();

            if (ImGui::BeginTabBar("EditMode", ImGuiTabBarFlags_None)) {
                if (ImGui::BeginTabItem("Live")) {
                    ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
                        if (!comp.HasComponent(liveLock, inspectTarget)) return;
                        if (ImGui::CollapsingHeader(comp.name, ImGuiTreeNodeFlags_DefaultOpen)) {
                            const void *component = comp.Access(liveLock, inspectTarget);
                            for (auto &field : comp.metadata.fields) {
                                ecs::GetFieldType(field.type, [&](auto *typePtr) {
                                    using T = std::remove_pointer_t<decltype(typePtr)>;
                                    // Set temp context for field controls
                                    this->scene = liveScene;
                                    this->target = inspectTarget;
                                    AddFieldControls<T>(field, comp, component);
                                });
                            }
                        }
                    });
                    ImGui::EndTabItem();
                }
                auto stagingId = sceneInfo.rootStagingId;
                while (stagingId.Has<ecs::SceneInfo>(stagingLock)) {
                    auto &stagingSceneInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                    auto stagingScene = stagingSceneInfo.scene.lock();
                    if (stagingScene) {
                        std::string tabName;
                        if (!stagingSceneInfo.prefabStagingId) {
                            tabName = "Scene: " + stagingScene->name;
                        } else {
                            Assertf(stagingSceneInfo.prefabStagingId.Has<ecs::Scripts>(stagingLock),
                                "SceneInfo.prefabStagingId does not have a Scripts component");
                            auto &prefabScripts = stagingSceneInfo.prefabStagingId.Get<ecs::Scripts>(stagingLock);
                            auto scriptInstance = prefabScripts.FindScript(stagingSceneInfo.prefabScriptId);
                            if (scriptInstance) {
                                if (scriptInstance->definition.name == "gltf") {
                                    tabName = "Gltf: " + scriptInstance->GetParam<std::string>("model") + " - " +
                                              ecs::ToString(stagingLock, stagingSceneInfo.prefabStagingId) + " - " +
                                              std::to_string(stagingId);
                                } else if (scriptInstance->definition.name == "template") {
                                    tabName = "Template: " + scriptInstance->GetParam<std::string>("source") + " - " +
                                              ecs::ToString(stagingLock, stagingSceneInfo.prefabStagingId) + " - " +
                                              std::to_string(stagingId);
                                } else {
                                    tabName = "Prefab: " + scriptInstance->definition.name + " - " +
                                              ecs::ToString(stagingLock, stagingSceneInfo.prefabStagingId) + " - " +
                                              std::to_string(stagingId);
                                }
                            } else {
                                tabName = "Prefab: null - " +
                                          ecs::ToString(stagingLock, stagingSceneInfo.prefabStagingId) + " - " +
                                          std::to_string(stagingId);
                            }
                        }
                        if (ImGui::BeginTabItem(tabName.c_str())) {
                            // if (ImGui::Button("Refresh Scene Prefabs")) {
                            //     GetSceneManager().QueueAction(SceneAction::RefreshScenePrefabs, stagingScene->name);
                            // }
                            // ImGui::SameLine();
                            if (ImGui::Button("Apply Scene")) {
                                // GetSceneManager().QueueAction(SceneAction::RefreshScenePrefabs, stagingScene->name);
                                GetSceneManager().QueueAction(SceneAction::ApplyStagingScene, stagingScene->name);
                            }
                            if (!stagingSceneInfo.prefabStagingId) {
                                ImGui::SameLine();
                                if (ImGui::Button("Save & Apply Scene")) {
                                    // GetSceneManager().QueueAction(SceneAction::RefreshScenePrefabs,
                                    // stagingScene->name);
                                    GetSceneManager().QueueAction(SceneAction::ApplyStagingScene, stagingScene->name);
                                    GetSceneManager().QueueAction(SceneAction::SaveStagingScene, stagingScene->name);
                                }
                            } else {
                                ImGui::SameLine();
                                if (ImGui::Button("Prefab Source")) {
                                    targetEntity = stagingSceneInfo.prefabStagingId;
                                }
                            }
                            ImGui::Separator();
                            ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
                                if (!comp.HasComponent(stagingLock, stagingId)) return;
                                if (ImGui::CollapsingHeader(comp.name, ImGuiTreeNodeFlags_DefaultOpen)) {
                                    const void *component = comp.Access(stagingLock, stagingId);
                                    for (auto &field : comp.metadata.fields) {
                                        ecs::GetFieldType(field.type, [&](auto *typePtr) {
                                            using T = std::remove_pointer_t<decltype(typePtr)>;
                                            // Set temp context for field controls
                                            this->scene = stagingScene;
                                            this->target = stagingId;
                                            AddFieldControls<T>(field, comp, component);
                                        });
                                    }
                                }
                            });
                            ImGui::EndTabItem();
                        }
                    } else {
                        Logf("Missing staging scene! %s", std::to_string(stagingId));
                    }

                    auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(stagingLock);
                    stagingId = stagingInfo.nextStagingId;
                }
                ImGui::EndTabBar();
            }
        } else {
            ImGui::Text("Missing Entity: %s", ecs::ToString(liveLock, inspectTarget).c_str());
        }
    }
} // namespace sp
