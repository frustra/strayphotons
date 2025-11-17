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
    using namespace ecs;

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
                        bool isSelected = item.first == value;
                        if (ImGui::Selectable(item.second.data(), isSelected)) {
                            value = item.first;
                            changed = true;
                        }

                        if (isSelected) ImGui::SetItemDefaultFocus();
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
                ImGui::TextUnformatted(jsonValue.serialize(true).c_str());
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
    bool EditorContext::AddImGuiElement(const std::string &name, InlineString<127> &value) {
        bool changed = ImGui::InputText(name.c_str(), value.data(), value.max_size());
        if (changed) value = InlineString<127>(value.data()); // Update size byte at end
        return changed;
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, SignalExpression &value) {
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
    bool EditorContext::AddImGuiElement(const std::string &name, EntityRef &value) {
        bool changed = false;
        if (!fieldName.empty()) {
            ImGui::Text("%s:", fieldName.c_str());
            ImGui::SameLine();
        }
        if (value) {
            std::string buttonLabel = "-##" + name;
            if (ImGui::Button(buttonLabel.c_str(), ImVec2(20, 0))) {
                value = {};
                changed = true;
            }
            ImGui::SameLine();
        }
        ImGui::Button(value ? value.Name().String().c_str() : "None");
        if (ImGui::BeginPopupContextItem(name.c_str(), ImGuiPopupFlags_MouseButtonLeft)) {
            EntityRef selectedRef;
            auto selected = ShowAllEntities(selectedRef, fieldId, 400, ImGui::GetTextLineHeightWithSpacing() * 25);
            if (selected) {
                value = selectedRef;
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        return changed;
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, Transform &value) {
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
    bool EditorContext::AddImGuiElement(const std::string &name, std::vector<AnimationState> &value) {
        for (auto &state : value) {
            picojson::value jsonValue;
            json::Save({}, jsonValue, state);
            ImGui::TextUnformatted(jsonValue.serialize(true).c_str());
        }
        return false;
    }
    template<>
    bool EditorContext::AddImGuiElement(const std::string &name, std::vector<ScriptInstance> &value) {
        bool changed = false;
        std::vector<size_t> removeList;
        robin_hood::unordered_map<size_t, std::string> changeList;
        for (auto &instance : value) {
            if (!instance || !instance.state) continue;
            auto &state = *instance.state;
            std::string rowId = fieldId + "." + std::to_string(state.GetInstanceId());
            std::string scriptLabel;
            switch (state.definition.type) {
            case ScriptType::LogicScript:
                if (state.definition.filterOnEvent) {
                    scriptLabel = "OnTick(filtered): " + state.definition.name;
                } else {
                    scriptLabel = "OnTick: " + state.definition.name;
                }
                break;
            case ScriptType::PhysicsScript:
                if (state.definition.filterOnEvent) {
                    scriptLabel = "OnPhysicsUpdateEvent: " + state.definition.name;
                } else {
                    scriptLabel = "OnPhysicsUpdate: " + state.definition.name;
                }
                break;
            case ScriptType::EventScript:
                scriptLabel = "OnEvent: " + state.definition.name;
                break;
            case ScriptType::PrefabScript:
                if (state.definition.name == "prefab_template") {
                    scriptLabel = "Template: " + state.GetParam<std::string>("source");
                } else if (state.definition.name == "prefab_gltf") {
                    scriptLabel = "Gltf: " + state.GetParam<std::string>("model");
                } else {
                    scriptLabel = "Prefab: " + state.definition.name;
                }
                break;
            case ScriptType::GuiScript:
                scriptLabel = "Gui: " + state.definition.name;
                break;
            default:
                scriptLabel = "Invalid script: " + state.definition.name;
                break;
            }
            if (state.definition.name.empty()) {
                scriptLabel += "(inline C++ lambda)";
            }

            auto scriptType = std::string(magic_enum::enum_name(state.definition.type));
            if (ImGui::TreeNodeEx(rowId.c_str(),
                    ImGuiTreeNodeFlags_DefaultOpen,
                    "%s %s",
                    scriptType.c_str(),
                    scriptLabel.c_str())) {
                if (IsLive(target) && state.definition.type == ScriptType::PrefabScript) {
                    ImGui::BeginDisabled();
                } else if (IsStaging(target)) {
                    if (ImGui::Button("-", ImVec2(20, 0))) {
                        removeList.emplace_back(state.GetInstanceId());
                    }
                    ImGui::SameLine();
                }
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::BeginCombo(rowId.c_str(), state.definition.name.c_str())) {
                    const auto &scripts = GetScriptDefinitions().scripts;
                    for (auto &[scriptName, definition] : scripts) {
                        if (IsLive(target) && definition.type != state.definition.type) {
                            // Don't allow change script types in the live ECS
                            continue;
                        }
                        bool isSelected = state.definition.name == scriptName;
                        if (ImGui::Selectable(scriptName.c_str(), isSelected)) {
                            if (!isSelected) changeList.emplace(state.GetInstanceId(), scriptName);
                            changed = true;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                auto ctx = state.definition.context.lock();
                if (ctx) {
                    std::shared_lock l1(GetScriptManager().dynamicLibraryMutex);
                    std::lock_guard l2(GetScriptManager().scripts[state.definition.type].mutex);
                    void *dataPtr = ctx->AccessMut(state);
                    auto &fields = ctx->metadata.fields;
                    if (dataPtr && !fields.empty()) {
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
                                    ImGui::TextUnformatted(field.name.c_str());
                                    ImGui::TableSetColumnIndex(1);
                                    GetFieldType(field.type, field.AccessMut(dataPtr), [&](auto &fieldValue) {
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
                if (IsLive(target) && state.definition.type == ScriptType::PrefabScript) {
                    ImGui::EndDisabled();
                }

                ImGui::TreePop();
            }
        }
        for (auto &[instanceId, scriptName] : changeList) {
            const auto &definition = GetScriptDefinitions().scripts.at(scriptName);
            for (auto &instance : value) {
                if (instance.GetInstanceId() == instanceId) {
                    instance = GetScriptManager().NewScriptInstance(instance.GetState().scope,
                        definition,
                        IsLive(target));
                    break;
                }
            }
        }
        if (IsStaging(target)) {
            for (auto &instanceId : removeList) {
                sp::erase_if(value, [&](auto &&instance) {
                    return instance.GetInstanceId() == instanceId;
                });
                changed = true;
            }
            if (ImGui::Button("Add Prefab")) {
                EntityScope scope = Name(scene.data->name, "");
                value.emplace_back(scope,
                    ScriptDefinition{"", ScriptType::PrefabScript, {}, false, {}, {}, {}, PrefabFunc()});
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add LogicScript")) {
                EntityScope scope = Name(scene.data->name, "");
                value.emplace_back(scope,
                    ScriptDefinition{"", ScriptType::LogicScript, {}, false, {}, {}, {}, LogicTickFunc()});
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Physics Script")) {
                EntityScope scope = Name(scene.data->name, "");
                value.emplace_back(scope,
                    ScriptDefinition{"", ScriptType::PhysicsScript, {}, false, {}, {}, {}, PhysicsTickFunc()});
                changed = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Event Script")) {
                EntityScope scope = Name(scene.data->name, "");
                value.emplace_back(scope,
                    ScriptDefinition{"", ScriptType::EventScript, {}, true, {}, {}, {}, OnEventFunc()});
                changed = true;
            }
        }
        return changed;
    }

    template<typename T>
    void EditorContext::AddFieldControls(const StructField &field, const ComponentBase &comp, const void *component) {
        auto value = field.Access<T>(component);
        fieldName = field.name;
        fieldId = "##"s + comp.name + std::to_string(field.fieldIndex);
        std::string elementName = fieldName + fieldId;

        bool valueChanged = false;
        bool isDefined = true;
        if constexpr (std::equality_comparable<T>) {
            if (IsStaging(target)) {
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
            if (IsLive(target)) {
                QueueTransaction<WriteAll>([target = this->target, value, &comp, &field](auto &lock) {
                    void *component = comp.AccessMut(lock, target);
                    field.Access<T>(component) = value;
                    if constexpr (std::is_same<T, std::vector<ScriptInstance>>()) {
                        GetScriptManager().RegisterEvents(lock);
                    }
                });
            } else if (scene) {
                QueueStagingTransaction<WriteAll>([target = this->target, value, &comp, &field](auto &lock) {
                    void *component = comp.AccessMut(lock, target);
                    field.Access<T>(component) = value;
                });
            } else {
                Errorf("Can't add ImGui field controls for null scene: %s", std::to_string(target));
            }
        }
    }

    template<typename... AllComponentTypes, template<typename...> typename ECSType>
    void CopyToStaging(Tecs::Lock<ECSType<AllComponentTypes...>, AddRemove> staging,
        Lock<ReadAll> live,
        Entity target) {

        Assertf(target.Has<SceneInfo>(live), "CopyToStaging target has no SceneInfo: %s", ToString(live, target));

        auto &liveSceneInfo = target.Get<SceneInfo>(live);
        auto stagingId = liveSceneInfo.rootStagingId;
        SceneRef targetScene;
        while (stagingId.Has<SceneInfo>(staging)) {
            auto &sceneInfo = stagingId.Get<SceneInfo>(staging);
            if (sceneInfo.priority == ScenePriority::Scene) {
                targetScene = sceneInfo.scene;
                break;
            }
            stagingId = sceneInfo.nextStagingId;
        }

        Assertf(targetScene, "CopyToStaging can't find suitable target scene: %s", liveSceneInfo.scene.data->name);
        Assertf(stagingId.Has<SceneInfo>(staging),
            "CopyToStaging can't find suitable target: %s / %s",
            ToString(live, target),
            liveSceneInfo.scene.data->name);
        auto &stagingInfo = stagingId.Get<SceneInfo>(staging);

        FlatEntity flatParentEntity;
        scene_util::BuildEntity(Lock<ReadAll>(staging), stagingInfo.nextStagingId, flatParentEntity);
        FlatEntity flatStagingEntity;

        ( // For each component:
            [&] {
                using T = AllComponentTypes;
                if constexpr (std::is_same_v<T, Name>) {
                    // Skip
                } else if constexpr (std::is_same_v<T, SceneInfo>) {
                    // Skip, this is handled by the scene
                } else if constexpr (std::is_same_v<T, SceneProperties>) {
                    // Skip, this is handled by scene
                } else if constexpr (std::is_same_v<T, TransformSnapshot>) {
                    // Skip, this is handled by TransformTree
                } else if constexpr (!Tecs::is_global_component<T>()) {
                    if (!target.Has<T>(live)) return;
                    auto &liveComp = target.Get<T>(live);
                    auto &comp = LookupComponent<T>();

                    T compareComp = {};
                    auto &existingComp = std::get<std::optional<T>>(flatParentEntity);
                    if (existingComp) {
                        comp.ApplyComponent(compareComp, *existingComp, true);
                    }

                    picojson::value tmp;
                    EntityScope scope(targetScene.data->name, "");
                    auto changed = json::SaveIfChanged(scope, tmp, "", liveComp, &compareComp);
                    if (changed) {
                        if (!comp.LoadEntity(flatStagingEntity, tmp)) {
                            Errorf("Failed to save %s component on entity: %s",
                                comp.name,
                                ToString(staging, stagingId));
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
