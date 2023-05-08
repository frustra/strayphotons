#pragma once

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "core/Defer.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/SignalRef.hh"
#include "game/SceneImpl.hh"
#include "game/SceneManager.hh"
#include "game/SceneRef.hh"

#include <glm/glm.hpp>
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
        std::string followFocus;
        int followFocusPos;

        // Temporary context
        const ecs::Lock<ecs::ReadAll> *lock = nullptr;
        std::string fieldName, fieldId;
        int signalNameCursorPos;

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

        void AddLiveSignalControls(const ecs::Lock<ecs::ReadAll> &lock, const ecs::EntityRef &targetEntity);
        void ShowEntityControls(const ecs::Lock<ecs::ReadAll> &lock, const ecs::EntityRef &targetEntity);
        void ShowSceneControls(const ecs::Lock<ecs::ReadAll> &lock);

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
        bool borderEnable = !value;
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
                    auto existingComp = std::get<std::optional<T>>(flatParentEntity);
                    if (existingComp) {
                        comp.ApplyComponent(compareComp, *existingComp, true);
                    }

                    picojson::value tmp;
                    ecs::EntityScope scope(targetScene.data->name, "");
                    auto changed = json::SaveIfChanged(scope, tmp, "", liveComp, compareComp);
                    if (changed) {
                        if (!comp.LoadEntity(staging, scope, stagingId, tmp)) {
                            Errorf("Failed to save %s component on entity: %s",
                                comp.name,
                                ecs::ToString(staging, stagingId));
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

    void EditorContext::AddLiveSignalControls(const ecs::Lock<ecs::ReadAll> &lock, const ecs::EntityRef &targetEntity) {
        Assertf(ecs::IsLive(lock), "AddLiveSignalControls must be called with a live lock");
        if (ImGui::CollapsingHeader("Signals", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
                                    ImGuiTableFlags_SizingStretchSame;
            if (ImGui::BeginTable("##SignalTable", 4, flags)) {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 20.0f);
                ImGui::TableSetupColumn("Signal");
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 20.0f);
                ImGui::TableSetupColumn("Binding");
                ImGui::TableHeadersRow();

                auto &signalManager = ecs::GetSignalManager();
                std::set<ecs::SignalRef> signals = signalManager.GetSignals(targetEntity);
                auto parentFieldId = fieldId;
                for (auto &ref : signals) {
                    if (!ref) continue;
                    bool hasValue = ref.HasValue(lock);
                    if (!hasValue && !ref.HasBinding(lock)) continue;

                    ImGui::PushID(ref.GetSignalName().c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Button("-", ImVec2(20, 0))) {
                        ecs::QueueTransaction<ecs::Write<ecs::Signals>>([ref = ref](auto &lock) {
                            ref.ClearValue(lock);
                            ref.ClearBinding(lock);
                        });
                    }
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    // A custom callback to track cursor position
                    // The field id changes when the signal is renamed, so we need to manually
                    // move the focus to the new field id
                    auto focusTracker = [](ImGuiInputTextCallbackData *data) -> int {
                        auto *ctx = (EditorContext *)data->UserData;
                        if (ctx->followFocus == ctx->fieldId) {
                            data->SelectionStart = data->SelectionEnd = data->CursorPos = ctx->followFocusPos;
                            ctx->followFocus = "";
                        } else if (ImGui::IsItemFocused()) {
                            ctx->signalNameCursorPos = data->CursorPos;
                        }
                        return 0;
                    };

                    auto signalName = ref.GetSignalName();
                    fieldId = "##SignalName." + ref.GetSignalName();
                    if (followFocus == fieldId) {
                        ImGui::SetKeyboardFocusHere();
                    }
                    if (ImGui::InputText(fieldId.c_str(),
                            &signalName,
                            ImGuiInputTextFlags_CallbackAlways,
                            focusTracker,
                            this)) {
                        ecs::SignalRef newRef(ref.GetEntity(), signalName);
                        if (newRef && !newRef.HasValue(lock) && !newRef.HasBinding(lock)) {
                            if (ImGui::IsItemFocused()) {
                                followFocus = "##SignalName." + signalName;
                                followFocusPos = signalNameCursorPos;
                            }

                            ecs::QueueTransaction<ecs::Write<ecs::Signals>>([ref = ref, newRef](auto &lock) {
                                if (ref.HasValue(lock)) {
                                    newRef.SetValue(lock, ref.GetValue(lock));
                                    ref.ClearValue(lock);
                                }
                                if (ref.HasBinding(lock)) {
                                    newRef.SetBinding(lock, ref.GetBinding(lock));
                                    ref.ClearBinding(lock);
                                }
                            });
                        }
                    }
                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::Checkbox("", &hasValue)) {
                        ecs::QueueTransaction<ecs::Write<ecs::Signals>>([hasValue, ref = ref](auto &lock) {
                            if (hasValue) {
                                ref.SetValue(lock, 0.0);
                            } else {
                                ref.ClearValue(lock);
                                if (!ref.HasBinding(lock)) ref.SetBinding(lock, "0.0");
                            };
                        });
                    }
                    ImGui::TableSetColumnIndex(3);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (hasValue) {
                        double signalValue = ref.GetValue(lock);
                        if (AddImGuiElement("##SignalValue." + ref.GetSignalName(), signalValue)) {
                            ecs::QueueTransaction<ecs::Write<ecs::Signals>>([ref = ref, signalValue](auto &lock) {
                                ref.SetValue(lock, signalValue);
                            });
                        }
                    } else {
                        ecs::SignalExpression expression = ref.GetBinding(lock);
                        if (AddImGuiElement("##SignalBinding." + ref.GetSignalName(), expression)) {
                            if (expression) {
                                ecs::QueueTransaction<ecs::Write<ecs::Signals>>([ref = ref, expression](auto &lock) {
                                    ref.SetBinding(lock, expression);
                                });
                            }
                        }
                    }

                    ImGui::PopID();
                }
                fieldId = parentFieldId;
            }
            ImGui::EndTable();

            if (ImGui::Button("Add Signal")) {
                ecs::QueueTransaction<ecs::Write<ecs::Signals>>([targetEntity](auto &lock) {
                    for (size_t i = 0;; i++) {
                        std::string signalName = i > 0 ? ("value" + std::to_string(i)) : "value";
                        ecs::SignalRef newRef(targetEntity, signalName);
                        if (!newRef.HasValue(lock) && !newRef.HasBinding(lock)) {
                            newRef.SetValue(lock, 0.0);
                            break;
                        }
                    }
                });
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Binding")) {
                ecs::QueueTransaction<ecs::Write<ecs::Signals>>([targetEntity](auto &lock) {
                    for (size_t i = 0;; i++) {
                        std::string signalName = i > 0 ? ("binding" + std::to_string(i)) : "binding";
                        ecs::SignalRef newRef(targetEntity, signalName);
                        if (!newRef.HasValue(lock) && !newRef.HasBinding(lock)) {
                            newRef.SetBinding(lock, "0 + 0");
                            break;
                        }
                    }
                });
            }
        }
    }

    void EditorContext::ShowEntityControls(const ecs::Lock<ecs::ReadAll> &lock, const ecs::EntityRef &targetEntity) {
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

            if (this->scene) {
                if (!sceneInfo.prefabStagingId) {
                    ImGui::SameLine();
                    if (ImGui::Button("Copy to Staging")) {
                        GetSceneManager().QueueAction([target = this->target] {
                            auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                            auto liveLock = ecs::StartTransaction<ecs::ReadAll>();

                            CopyToStaging(stagingLock, liveLock, target);
                        });
                    }
                }
            }

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
                        GetSceneManager().QueueAction(SceneAction::ApplyResetStagingScene, this->scene.data->name);
                    }
                    if (!targetSceneInfo.prefabStagingId) {
                        ImGui::SameLine();
                        if (ImGui::Button("Save & Apply Scene")) {
                            GetSceneManager().QueueAction(SceneAction::RefreshScenePrefabs, this->scene.data->name);
                            GetSceneManager().QueueAction(SceneAction::ApplyResetStagingScene, this->scene.data->name);
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

        if (ecs::IsLive(lock)) AddLiveSignalControls(lock, targetEntity);

        ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
            if (!comp.HasComponent(lock, this->target)) return;
            auto flags = (name == "scene_properties") ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen;
            if (ImGui::CollapsingHeader(name.c_str(), flags)) {
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

    void EditorContext::ShowSceneControls(const ecs::Lock<ecs::ReadAll> &lock) {
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
                    ecs::GetFieldType(field.type, field.Access(&properties), [&](auto &value) {
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
