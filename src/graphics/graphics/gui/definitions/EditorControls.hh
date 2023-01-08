#pragma once

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "core/Defer.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/SignalExpression.hh"
#include "game/SceneManager.hh"
#include "game/SceneRef.hh"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <magic_enum.hpp>
#include <map>

namespace sp {
    struct EditorContext {
        // Persistent context
        struct TreeNode {
            bool hasParent = false;
            std::vector<ecs::Name> children;
        };
        std::string entitySearch, sceneEntry;
        std::map<ecs::Name, TreeNode> entityTree;
        SceneRef scene;
        ecs::Entity target;

        // Temporary context
        ecs::Lock<ecs::ReadAll> *lock = nullptr;
        std::string fieldName, fieldId;

        void RefreshEntityTree() {
            entityTree.clear();

            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::TransformTree>>();
            for (auto &ent : lock.EntitiesWith<ecs::TransformTree>()) {
                auto &name = ent.Get<ecs::Name>(lock);
                auto &tree = ent.Get<ecs::TransformTree>(lock);
                entityTree[name].hasParent = (bool)tree.parent;
                if (tree.parent) {
                    entityTree[tree.parent.Name()].children.emplace_back(name);
                }
            }
        }

        ecs::EntityRef ShowEntityTree(ecs::Name root = ecs::Name()) {
            ecs::EntityRef selected;
            if (!root) {
                if (ImGui::Button("Refresh List") || entityTree.empty()) {
                    RefreshEntityTree();
                }
                for (auto &pair : entityTree) {
                    if (!pair.second.hasParent) {
                        auto tmp = ShowEntityTree(pair.first);
                        if (tmp) selected = tmp;
                    }
                }
            } else {
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
                if (entityTree[root].children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;

                bool open = ImGui::TreeNodeEx(root.String().c_str(), flags);
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    selected = root;
                }
                if (!open) return selected;

                for (auto &child : entityTree[root].children) {
                    if (child) {
                        auto tmp = ShowEntityTree(child);
                        if (tmp) selected = tmp;
                    }
                }

                ImGui::TreePop();
            }
            return selected;
        }

        ecs::EntityRef ShowAllEntities(std::string listLabel, float listWidth = -FLT_MIN, float listHeight = -FLT_MIN) {
            ecs::EntityRef selected;
            ImGui::SetNextItemWidth(listWidth);
            ImGui::InputTextWithHint("##entity_search", "Entity Search", &entitySearch);
            if (ImGui::BeginListBox(listLabel.c_str(), ImVec2(listWidth, listHeight))) {
                auto entityNames = ecs::GetEntityRefs().GetNames(entitySearch);
                for (auto &entName : entityNames) {
                    if (ImGui::Selectable(entName.String().c_str())) {
                        selected = entName;
                    }
                }
                ImGui::EndListBox();
            }
            return selected;
        }

        void ShowEntityControls(ecs::Lock<ecs::ReadAll> lock, ecs::EntityRef targetEntity);
        void ShowSceneControls(ecs::Lock<ecs::ReadAll> lock);

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
            auto selected = ShowAllEntities(fieldId, 400, ImGui::GetTextLineHeightWithSpacing() * 25);
            if (selected) {
                value = selected;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        return changed;
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, ecs::Transform &value) {
        // TODO: Add grab handle in view
        bool groupTransformControls = !fieldName.empty();
        if (groupTransformControls) {
            float frameHeight = ImGui::GetStyle().FramePadding.y * 2;
            frameHeight += ImGui::GetFrameHeightWithSpacing() * 4; // 4 rows of controls
            ImGui::BeginChild(name.c_str(), ImVec2(-FLT_MIN, frameHeight), true);
            ImGui::Text("%s:", fieldName.c_str());
        }
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
            if (glm::all(glm::notEqual(scale, glm::vec3(0.0f)))) {
                value.SetScale(scale);
                changed = true;
            }
        }
        if (groupTransformControls) ImGui::EndChild();
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
                if (ecs::IsLive(target) && isPrefab) {
                    ImGui::BeginDisabled();
                } else if (ecs::IsStaging(target)) {
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
                                                ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame;
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
                if (ecs::IsLive(target) && isPrefab) {
                    ImGui::EndDisabled();
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
                state.scope = ecs::Name(scene.data->name, "");
                state.definition.callback = ecs::PrefabFunc();
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Script")) {
                auto &state = value.emplace_back();
                state.scope = ecs::Name(scene.data->name, "");
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
        fieldName = field.name ? field.name : "";
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
                ecs::QueueTransaction<ecs::WriteAll>([target = this->target, value, &comp, &field](auto lock) {
                    void *component = comp.Access(lock, target);
                    *field.Access<T>(component) = value;
                });
            } else if (scene) {
                GetSceneManager().QueueAction(SceneAction::EditStagingScene,
                    scene.data->name,
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

    void EditorContext::ShowEntityControls(ecs::Lock<ecs::ReadAll> lock, ecs::EntityRef targetEntity) {
        if (!targetEntity) {
            this->target = {};
            return;
        }

        this->lock = &lock;
        Defer defer([this] {
            // Clear temporary context
            this->lock = nullptr;
            this->fieldName = "";
            this->fieldId = "";
        });

        if (ecs::IsLive(lock)) {
            this->target = targetEntity.GetLive();
        } else if (ecs::IsStaging(lock)) {
            if (ecs::IsStaging(this->target) && this->target.Has<ecs::Name>(lock)) {
                auto &currentName = this->target.Get<ecs::Name>(lock);
                if (currentName != targetEntity.Name()) {
                    this->target = targetEntity.GetStaging();
                }
            } else {
                this->target = targetEntity.GetStaging();
            }
        } else {
            Abortf("Unexpected lock passed to EditorContext::ShowEntityControls: instance id%u",
                lock.GetInstance().GetInstanceId());
        }

        if (!this->target.Has<ecs::SceneInfo>(lock)) {
            ImGui::Text("Missing Entity: %s", ecs::ToString(lock, this->target).c_str());
            return;
        }

        if (ecs::IsLive(this->target)) {
            auto &sceneInfo = this->target.Get<ecs::SceneInfo>(lock);
            this->scene = sceneInfo.scene;

            ImGui::Separator();
            ImGui::Text("Entity: %s", ecs::ToString(lock, this->target).c_str());
        } else {
            if (this->target.Has<ecs::SceneInfo>(lock)) {
                auto &targetSceneInfo = this->target.Get<ecs::SceneInfo>(lock);
                this->scene = targetSceneInfo.scene;

                if (this->scene) {
                    ImGui::SameLine();
                    if (ImGui::Button("Apply Scene")) {
                        GetSceneManager().QueueAction(SceneAction::RefreshScenePrefabs, this->scene.data->name);
                        GetSceneManager().QueueAction(SceneAction::ApplyStagingScene, this->scene.data->name);
                    }
                    if (!targetSceneInfo.prefabStagingId) {
                        ImGui::SameLine();
                        if (ImGui::Button("Save & Apply Scene")) {
                            GetSceneManager().QueueAction(SceneAction::RefreshScenePrefabs, this->scene.data->name);
                            GetSceneManager().QueueAction(SceneAction::ApplyStagingScene, this->scene.data->name);
                            GetSceneManager().QueueAction(SceneAction::SaveStagingScene, this->scene.data->name);
                        }
                    }
                }

                ImGui::Separator();
                ImGui::Text("Entity: %s", ecs::ToString(lock, this->target).c_str());

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Entity Definitions:");
                if (targetSceneInfo.prefabStagingId) {
                    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().FramePadding.x - 200.0f);
                    if (ImGui::Button("Goto Prefab Source", ImVec2(200, 0))) {
                        this->target = targetSceneInfo.prefabStagingId;
                    }
                }
                if (ImGui::BeginListBox("##EntitySource",
                        ImVec2(-FLT_MIN, 4.25 * ImGui::GetTextLineHeightWithSpacing()))) {
                    auto stagingId = targetSceneInfo.rootStagingId;
                    while (stagingId.Has<ecs::SceneInfo>(lock)) {
                        auto &sceneInfo = stagingId.Get<ecs::SceneInfo>(lock);
                        if (sceneInfo.scene) {
                            std::string sourceName;
                            if (!sceneInfo.prefabStagingId) {
                                sourceName = sceneInfo.scene.data->name + " - Scene Root";
                            } else {
                                const ecs::ScriptState *scriptInstance = nullptr;
                                if (sceneInfo.prefabStagingId.Has<ecs::Scripts>(lock)) {
                                    auto &prefabScripts = sceneInfo.prefabStagingId.Get<ecs::Scripts>(lock);
                                    scriptInstance = prefabScripts.FindScript(sceneInfo.prefabScriptId);
                                }
                                if (scriptInstance) {
                                    if (scriptInstance->definition.name == "gltf") {
                                        sourceName = scriptInstance->GetParam<std::string>("model") + " - Gltf Model";
                                    } else if (scriptInstance->definition.name == "template") {
                                        sourceName = scriptInstance->GetParam<std::string>("source") + " - Template";
                                    } else {
                                        sourceName = scriptInstance->definition.name + " - Prefab";
                                    }
                                } else {
                                    sourceName = "Null Prefab";
                                }
                            }

                            if (ImGui::Selectable(sourceName.c_str(), this->target == stagingId)) {
                                this->target = stagingId;
                            }
                        } else {
                            ImGui::TextColored({1, 0, 0, 1},
                                "Missing staging scene! %s",
                                std::to_string(stagingId).c_str());
                        }

                        auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(lock);
                        stagingId = stagingInfo.nextStagingId;
                    }
                    ImGui::EndListBox();
                }
            }
        }

        ImGui::Separator();

        ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
            if (!comp.HasComponent(lock, this->target)) return;
            if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                const void *component = comp.Access(lock, this->target);
                for (auto &field : comp.metadata.fields) {
                    ecs::GetFieldType(field.type, [&](auto *typePtr) {
                        using T = std::remove_pointer_t<decltype(typePtr)>;
                        AddFieldControls<T>(field, comp, component);
                    });
                }
            }
        });
    }

    void EditorContext::ShowSceneControls(ecs::Lock<ecs::ReadAll> lock) {
        ImGui::Text("Active Scene List:");
        if (ImGui::BeginListBox("##ActiveScenes", ImVec2(-FLT_MIN, 10.25 * ImGui::GetTextLineHeightWithSpacing()))) {
            auto sceneList = GetSceneManager().GetActiveScenes();
            std::sort(sceneList.begin(), sceneList.end());
            for (auto &entry : sceneList) {
                if (!entry || entry.data->type == SceneType::System) continue;
                std::string text = entry.data->name + " (" + std::string(magic_enum::enum_name(entry.data->type)) + ")";
                if (ImGui::Selectable(text.c_str(), entry == this->scene)) {
                    this->scene = entry;
                }
            }
            ImGui::EndListBox();
        }
        if (ImGui::Button("Reload All")) {
            GetSceneManager().QueueAction(SceneAction::ReloadScene);
        }
        ImGui::SameLine();
        bool openLoadScene = ImGui::Button("Load Scene");
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
            ImGui::SetNextItemWidth(300);
            if (openLoadScene) ImGui::SetKeyboardFocusHere();
            bool submit = ImGui::InputTextWithHint("##scene_name",
                "Scene Name",
                &sceneEntry,
                ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button("Load") || submit) {
                GetSceneManager().QueueAction(SceneAction::LoadScene, sceneEntry);
                sceneEntry.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        bool openAddScene = ImGui::Button("Add Scene");
        if (ImGui::BeginPopupContextItem(nullptr, ImGuiPopupFlags_MouseButtonLeft)) {
            ImGui::SetNextItemWidth(300);
            if (openAddScene) ImGui::SetKeyboardFocusHere();
            bool submit = ImGui::InputTextWithHint("##scene_name",
                "Scene Name",
                &sceneEntry,
                ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button("Add") || submit) {
                GetSceneManager().QueueAction(SceneAction::AddScene, sceneEntry);
                sceneEntry.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (this->scene) {
            ImGui::SameLine();
            if (ImGui::Button("Remove Scene")) {
                GetSceneManager().QueueAction(SceneAction::RemoveScene, this->scene.data->name);
            }
        }
        ImGui::Separator();
        if (this->scene) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Scene Name: %s", this->scene.data->name.c_str());
            if (this->scene.data->type != SceneType::System) {
                ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x * 2.0f - 120.0f);
                if (ImGui::Button("Reload", ImVec2(60, 0))) {
                    GetSceneManager().QueueAction(SceneAction::ReloadScene, this->scene.data->name);
                }
                ImGui::SameLine();
                if (ImGui::Button("Save", ImVec2(60, 0))) {
                    GetSceneManager().QueueAction(SceneAction::SaveStagingScene, this->scene.data->name);
                }
            }
            if (ImGui::CollapsingHeader("Scene Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto &metadata = ecs::StructMetadata::Get<ecs::SceneProperties>();

                ecs::SceneProperties properties = this->scene.data->GetProperties(lock);
                bool changed = false;
                for (auto &field : metadata.fields) {
                    fieldName = field.name;
                    fieldId = "##scene_properties" + std::to_string(field.fieldIndex);
                    std::string elementName = fieldName + fieldId;
                    ecs::GetFieldType(field.type, [&](auto *typePtr) {
                        using T = std::remove_pointer_t<decltype(typePtr)>;
                        auto &value = *field.Access<T>(&properties);
                        if (AddImGuiElement(elementName, value)) changed = true;
                    });
                }
                if (changed) {
                    GetSceneManager().QueueAction(SceneAction::EditStagingScene,
                        this->scene.data->name,
                        [properties](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                            auto sceneId = scene->data->sceneEntity.Get(lock);
                            if (sceneId.Exists(lock)) {
                                sceneId.Set<ecs::SceneProperties>(lock, properties);
                            }
                        });
                }
            }
            if (ImGui::CollapsingHeader("Scene Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::BeginListBox("##scene_entities", ImVec2(-FLT_MIN, -FLT_MIN))) {
                    auto entityNames = ecs::GetEntityRefs().GetNames(entitySearch);
                    for (auto &ent : lock.EntitiesWith<ecs::SceneInfo>()) {
                        if (!ent.Has<ecs::SceneInfo, ecs::Name>(lock)) continue;
                        auto &sceneInfo = ent.Get<ecs::SceneInfo>(lock);
                        if (sceneInfo.scene != this->scene) continue;

                        auto &name = ent.Get<ecs::Name>(lock);
                        if (ImGui::Selectable(name.String().c_str(), ent == this->target)) {
                            this->target = ent;
                        }
                    }
                    ImGui::EndListBox();
                }
            }
        }
    }
} // namespace sp
