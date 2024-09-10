/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "EditorControls.hh"
#include "assets/JsonHelpers.hh"
#include "common/Common.hh"
#include "common/Defer.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalRef.hh"
#include "ecs/StructFieldTypes.hh"
#include "game/SceneImpl.hh"
#include "game/SceneManager.hh"
#include "game/SceneRef.hh"
#include "input/BindingNames.hh"

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <magic_enum.hpp>
#include <map>

namespace sp {
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
        } else if constexpr (is_glm_vec<T>()) {
            using U = typename T::value_type;
            if constexpr (std::is_same_v<U, float>) {
                changed = ImGui::DragScalarN(name.c_str(),
                    ImGuiDataType_Float,
                    &value,
                    T::length(),
                    0.01f,
                    nullptr,
                    nullptr,
                    "%.4f");
            } else if constexpr (std::is_same_v<U, double>) {
                changed = ImGui::DragScalarN(name.c_str(),
                    ImGuiDataType_Double,
                    &value,
                    T::length(),
                    0.01f,
                    nullptr,
                    nullptr,
                    "%.4f");
            } else if constexpr (std::is_same_v<U, int>) {
                changed = ImGui::DragScalarN(name.c_str(),
                    ImGuiDataType_S32,
                    &value,
                    T::length(),
                    1.0f,
                    nullptr,
                    nullptr,
                    "%d");
            } else if constexpr (std::is_same_v<U, uint32_t>) {
                changed = ImGui::DragScalarN(name.c_str(),
                    ImGuiDataType_U32,
                    &value,
                    T::length(),
                    1.0f,
                    nullptr,
                    nullptr,
                    "%u");
            } else {
                static_assert(!sizeof(U), "AddImGuiElement unsupported vector type");
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
    bool EditorContext::AddImGuiElement(const std::string &name, double &value) {
        float floatValue = value;
        if (ImGui::DragFloat(name.c_str(), &floatValue, 0.01f)) {
            value = floatValue;
            return true;
        }
        return false;
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
    bool EditorContext::AddImGuiElement(const std::string &name, glm::quat &value) {
        // TODO: Add grab handle for orientation
        glm::vec3 angles = glm::degrees(glm::eulerAngles(value));
        for (glm::length_t i = 0; i < angles.length(); i++) {
            if (angles[i] < 0.0f) angles[i] += 360.0f;
            if (angles[i] >= 360.0f) angles[i] -= 360.0f;
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
        bool borderEnable = !value && !value.IsNull();
        if (borderEnable) {
            ImGui::PushStyleColor(ImGuiCol_Border, {1, 0, 0, 1});
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2);
        }
        bool changed = ImGui::InputText(name.c_str(), &value.expr);
        if (changed) value.Compile();
        if (borderEnable) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
        return changed;
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
        bool changed = ImGui::DragFloat3(text.c_str(), (float *)&value.offset[3], 0.01f);

        text = "rotation" + fieldId;
        glm::quat rotation = value.GetRotation();
        if (AddImGuiElement(text, rotation)) {
            value.SetRotation(rotation);
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
    bool EditorContext::AddImGuiElement(const std::string &name, std::vector<ecs::ScriptInstance> &value) {
        bool changed = false;
        std::vector<size_t> removeList;
        for (auto &instance : value) {
            if (!instance || !instance.state) continue;
            auto &state = *instance.state;
            std::string rowId = fieldId + "." + std::to_string(state.GetInstanceId());
            bool isOnTick = std::holds_alternative<ecs::OnTickFunc>(state.definition.callback);
            bool isOnPhysicsUpdate = std::holds_alternative<ecs::OnPhysicsUpdateFunc>(state.definition.callback);
            bool isPrefab = std::holds_alternative<ecs::PrefabFunc>(state.definition.callback);
            std::string scriptLabel;
            if (isOnTick) {
                if (state.definition.filterOnEvent) {
                    scriptLabel = "OnEvent: " + state.definition.name;
                } else {
                    scriptLabel = "OnTick: " + state.definition.name;
                }
            } else if (isOnPhysicsUpdate) {
                if (state.definition.filterOnEvent) {
                    scriptLabel = "OnPhysicsUpdateEvent: " + state.definition.name;
                } else {
                    scriptLabel = "OnPhysicsUpdate: " + state.definition.name;
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

            std::lock_guard l(ecs::GetScriptManager().mutexes[state.definition.callback.index()]);

            if (ImGui::TreeNodeEx(rowId.c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s", scriptLabel.c_str())) {
                if (ecs::IsLive(target) && isPrefab) {
                    ImGui::BeginDisabled();
                } else if (ecs::IsStaging(target)) {
                    if (ImGui::Button("-", ImVec2(20, 0))) {
                        removeList.emplace_back(state.GetInstanceId());
                    }
                    ImGui::SameLine();
                }
                if (isOnTick || isOnPhysicsUpdate) {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::BeginCombo(rowId.c_str(), state.definition.name.c_str())) {
                        auto &scripts = ecs::GetScriptDefinitions().scripts;
                        for (auto &[scriptName, definition] : scripts) {
                            // Don't allow changing the callback type, it will break ScriptManager's index
                            if (definition.callback.index() != state.definition.callback.index()) continue;
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
                                if (!field.name.empty()) {
                                    ImGui::TableNextRow();
                                    ImGui::TableSetColumnIndex(0);
                                    ImGui::Text("%s", field.name.c_str());
                                    ImGui::TableSetColumnIndex(1);
                                    ecs::GetFieldType(field.type, field.Access(dataPtr), [&](auto &fieldValue) {
                                        auto parentFieldName = fieldName;
                                        fieldName = "";
                                        ImGui::SetNextItemWidth(-FLT_MIN);
                                        if (AddImGuiElement(rowId + "."s + field.name, fieldValue)) {
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
                sp::erase_if(value, [&](auto &&instance) {
                    return instance.GetInstanceId() == instanceId;
                });
                changed = true;
            }
            if (ImGui::Button("Add Prefab")) {
                ecs::EntityScope scope = ecs::Name(scene.data->name, "");
                ecs::ScriptDefinition definition;
                definition.callback = ecs::PrefabFunc();
                value.emplace_back(scope, definition);
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add OnTick")) {
                ecs::EntityScope scope = ecs::Name(scene.data->name, "");
                ecs::ScriptDefinition definition;
                definition.callback = ecs::OnTickFunc();
                value.emplace_back(scope, definition);
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add OnPhysicsUpdate")) {
                ecs::EntityScope scope = ecs::Name(scene.data->name, "");
                ecs::ScriptDefinition definition;
                definition.callback = ecs::OnPhysicsUpdateFunc();
                value.emplace_back(scope, definition);
                changed = true;
            }
        }
        return changed;
    }

    template<typename T>
    void EditorContext::AddFieldControls(const ecs::StructField &field,
        const ecs::ComponentBase &comp,
        const void *component) {
        auto value = field.Access<T>(component);
        fieldName = field.name;
        fieldId = "##"s + comp.name + std::to_string(field.fieldIndex);
        std::string elementName = fieldName + fieldId;

        bool valueChanged = false;
        bool isDefined = true;
        if constexpr (std::equality_comparable<T>) {
            if (ecs::IsStaging(target)) {
                auto defaultLiveComponent = comp.GetLiveDefault();
                auto defaultStagingComponent = comp.GetStagingDefault();
                auto &defaultValue = field.Access<T>(defaultLiveComponent);
                auto &undefinedValue = field.Access<T>(defaultStagingComponent);
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
        }
        if (isDefined) {
            if (AddImGuiElement(elementName, value)) valueChanged = true;
        }

        if (valueChanged) {
            if (ecs::IsLive(target)) {
                ecs::QueueTransaction<ecs::WriteAll>([target = this->target, value, &comp, &field](auto &lock) {
                    void *component = comp.Access(lock, target);
                    field.Access<T>(component) = value;
                });
            } else if (scene) {
                GetSceneManager().QueueAction(SceneAction::EditStagingScene,
                    scene.data->name,
                    [target = this->target, value, &comp, &field](ecs::Lock<ecs::AddRemove> lock,
                        std::shared_ptr<Scene> scene) {
                        void *component = comp.Access((ecs::Lock<ecs::WriteAll>)lock, target);
                        field.Access<T>(component) = value;
                    });
            } else {
                Errorf("Can't add ImGui field controls for null scene: %s", std::to_string(target));
            }
        }
    }

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    void CopyToStaging(Tecs::Lock<ECSType<AllComponentTypes...>, ecs::AddRemove> staging,
        ecs::Lock<ecs::ReadAll> live,
        ecs::Entity target) {

        Assertf(target.Has<ecs::SceneInfo>(live),
            "CopyToStaging target has no SceneInfo: %s",
            ecs::ToString(live, target));

        auto &liveSceneInfo = target.Get<ecs::SceneInfo>(live);
        auto stagingId = liveSceneInfo.rootStagingId;
        SceneRef targetScene;
        while (stagingId.Has<ecs::SceneInfo>(staging)) {
            auto &sceneInfo = stagingId.Get<ecs::SceneInfo>(staging);
            if (sceneInfo.priority == ScenePriority::Scene) {
                targetScene = sceneInfo.scene;
                break;
            }
            stagingId = sceneInfo.nextStagingId;
        }

        Assertf(targetScene, "CopyToStaging can't find suitable target scene: %s", liveSceneInfo.scene.data->name);
        Assertf(stagingId.Has<ecs::SceneInfo>(staging),
            "CopyToStaging can't find suitable target: %s / %s",
            ecs::ToString(live, target),
            liveSceneInfo.scene.data->name);
        auto &stagingInfo = stagingId.Get<ecs::SceneInfo>(staging);

        auto flatParentEntity = scene::BuildEntity(ecs::Lock<ecs::ReadAll>(staging), stagingInfo.nextStagingId);
        ecs::FlatEntity flatStagingEntity;

        ( // For each component:
            [&] {
                using T = AllComponentTypes;
                if constexpr (std::is_same_v<T, ecs::Name>) {
                    // Skip
                } else if constexpr (std::is_same_v<T, ecs::SceneInfo>) {
                    // Skip, this is handled by the scene
                } else if constexpr (std::is_same_v<T, ecs::SceneProperties>) {
                    // Skip, this is handled by scene
                } else if constexpr (std::is_same_v<T, ecs::TransformSnapshot>) {
                    // Skip, this is handled by TransformTree
                } else if constexpr (!Tecs::is_global_component<T>()) {
                    if (!target.Has<T>(live)) return;
                    auto &liveComp = target.Get<T>(live);
                    auto comp = ecs::LookupComponent<T>();

                    T compareComp = {};
                    auto &existingComp = std::get<std::shared_ptr<T>>(flatParentEntity);
                    if (existingComp) {
                        comp.ApplyComponent(compareComp, *existingComp, true);
                    }

                    picojson::value tmp;
                    ecs::EntityScope scope(targetScene.data->name, "");
                    auto changed = json::SaveIfChanged(scope, tmp, "", liveComp, &compareComp);
                    if (changed) {
                        if (!comp.LoadEntity(flatStagingEntity, tmp)) {
                            Errorf("Failed to save %s component on entity: %s",
                                comp.name,
                                ecs::ToString(staging, stagingId));
                        } else {
                            comp.SetComponent(staging, scope, stagingId, flatStagingEntity);
                        }
                    } else if (existingComp) {
                        if (stagingId.Has<T>(staging)) stagingId.Unset<T>(staging);
                    } else {
                        stagingId.Set<T>(staging, comp.StagingDefault());
                    }
                }
            }(),
            ...);
    }
} // namespace sp
