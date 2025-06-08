/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EditorControlsImpl.hh"

namespace sp {
    void EditorContext::RefreshEntityTree() {
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

    ecs::EntityRef EditorContext::ShowEntityTree(ecs::Name root) {
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

    ecs::EntityRef EditorContext::ShowAllEntities(std::string listLabel, float listWidth, float listHeight) {
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

    void EditorContext::AddLiveSignalControls(const ecs::Lock<ecs::ReadAll> &lock, const ecs::EntityRef &targetEntity) {
        Assertf(ecs::IsLive(lock), "AddLiveSignalControls must be called with a live lock");
        if (ImGui::CollapsingHeader("Signals", ImGuiTreeNodeFlags_DefaultOpen)) {
            std::set<ecs::SignalRef> signals = ecs::GetSignalManager().GetSignals(targetEntity);

            // Make a best guess at the scope of this entity
            ecs::EntityScope scope(targetEntity.Name().scene, "");
            bool foundExact = false;
            for (auto &ref : signals) {
                if (!ref) continue;
                if (ref.HasBinding(lock)) {
                    scope = ref.GetBinding(lock).scope;
                    foundExact = true;
                    break;
                }
            }
            if (!foundExact && target.Has<ecs::SceneInfo>(lock)) {
                auto &sceneInfo = target.Get<ecs::SceneInfo>(lock);
                if (sceneInfo.prefabStagingId.Has<ecs::Name>(lock)) {
                    scope = sceneInfo.prefabStagingId.Get<ecs::Name>(lock);
                }
            }

            ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable |
                                    ImGuiTableFlags_SizingStretchSame;
            if (ImGui::BeginTable("##SignalTable", 4, flags)) {
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 20.0f);
                ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 20.0f);
                ImGui::TableSetupColumn("Value/Binding");
                ImGui::TableHeadersRow();

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

                            ecs::QueueTransaction<ecs::Write<ecs::Signals>, ecs::ReadSignalsLock>(
                                [ref = ref, newRef](auto &lock) {
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
                        ecs::QueueTransaction<ecs::Write<ecs::Signals>, ecs::ReadSignalsLock>(
                            [hasValue, ref = ref, scope](auto &lock) {
                                if (hasValue) {
                                    ref.SetValue(lock, 0.0);
                                } else {
                                    ref.ClearValue(lock);
                                    if (!ref.HasBinding(lock)) ref.SetBinding(lock, "0.0", scope);
                                };
                            });
                    }
                    ImGui::TableSetColumnIndex(3);
                    if (hasValue) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        double signalValue = ref.GetValue(lock);
                        if (AddImGuiElement("##SignalValue." + ref.GetSignalName(), signalValue)) {
                            ecs::QueueTransaction<ecs::Write<ecs::Signals>>([ref = ref, signalValue](auto &lock) {
                                ref.SetValue(lock, signalValue);
                            });
                        }
                    } else {
                        ImGui::SetNextItemWidth(-80.0f);
                        ecs::SignalExpression expression = ref.GetBinding(lock);
                        if (AddImGuiElement("##SignalBinding." + ref.GetSignalName(), expression)) {
                            if (expression) {
                                ecs::QueueTransaction<ecs::Write<ecs::Signals>, ecs::ReadSignalsLock>(
                                    [ref = ref, expression](auto &lock) {
                                        ref.SetBinding(lock, expression);
                                    });
                            }
                        }
                        ImGui::SameLine();
                        double value = expression.Evaluate(lock);
                        ImGui::Text("= %.4f", value);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%.16g", value);
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
                ecs::QueueTransaction<ecs::Write<ecs::Signals>, ecs::ReadSignalsLock>(
                    [targetEntity, scope](auto &lock) {
                        for (size_t i = 0;; i++) {
                            std::string signalName = i > 0 ? ("binding" + std::to_string(i)) : "binding";
                            ecs::SignalRef newRef(targetEntity, signalName);
                            if (!newRef.HasValue(lock) && !newRef.HasBinding(lock)) {
                                newRef.SetBinding(lock, "0 + 0", scope);
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
                ImGui::Text("Entity Definitions (Overrides First):");
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
                        if constexpr (std::is_default_constructible<T>()) {
                            AddFieldControls<T>(field, comp, component);
                        }
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
                    ecs::GetFieldType(field.type, field.AccessMut(&properties), [&](auto &value) {
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
                            ecs::QueueTransaction<ecs::SendEventsLock>([this, ent](auto &lock) {
                                ecs::EventBindings::SendEvent(lock,
                                    this->inspectorEntity,
                                    ecs::Event{EDITOR_EVENT_EDIT_TARGET, this->inspectorEntity.Get(lock), ent});
                            });
                        }
                    }
                    ImGui::EndListBox();
                }
            }
        }
    }

    void EditorContext::ShowSignalControls(const ecs::Lock<ecs::ReadAll> &lock) {
        ImGui::Text("Signal References:");
        ImGui::InputTextWithHint("##signal_search", "Signal Search", &signalSearch);
        if (ImGui::BeginListBox("##SignalRefs", ImVec2(-FLT_MIN, 10.25 * ImGui::GetTextLineHeightWithSpacing()))) {
            auto refList = ecs::GetSignalManager().GetSignals(signalSearch);
            for (auto &entry : refList) {
                std::string text = entry.String();
                // + " (" + std::string(magic_enum::enum_name(entry.data->type)) + ")";
                if (ImGui::Selectable(text.c_str(), entry == this->selectedSignal)) {
                    this->selectedSignal = entry;
                }
            }
            ImGui::EndListBox();
        }
        if (ImGui::Button("Drop Unused")) {
            ecs::GetSignalManager().DropAllUnusedRefs();
        }
        ImGui::Separator();
        if (this->selectedSignal) {
            ImGui::AlignTextToFramePadding();
            auto &ref = this->selectedSignal;
            std::string text = ref.String();
            ImGui::Text("Signal: %s", text.c_str());
            if (ref.HasValue(lock)) {
                ImGui::Text("Value = %.4f", ref.GetValue(lock));
            } else if (!ref.HasBinding(lock)) {
                ImGui::Text("Value = 0.0 (unset)");
            }
            if (ref.HasBinding(lock)) {
                auto &binding = ref.GetBinding(lock);
                ImGui::Text("Binding%s: %s", ref.HasValue(lock) ? " (overriden by value)" : "", binding.expr.c_str());
                if (binding.rootNode->text != binding.expr) {
                    ImGui::Text("Binding node: %s", binding.rootNode->text.c_str());
                }
                ImGui::Text("Binding eval = %.4f", binding.Evaluate(lock));
            }
            auto &signals = lock.Get<ecs::Signals>().signals;
            auto &index = ref.GetIndex();
            if (index < signals.size()) {
                auto &signal = signals[index];
                if (ref.IsCacheable(lock)) {
                    ImGui::Text("Cached value: %.4f %s", signal.lastValue, signal.lastValueDirty ? " (dirty)" : "");
                } else {
                    ImGui::Text("Signal uncacheable");
                }
                auto node = ecs::GetSignalManager().FindSignalNode(ref);
                if (node) {
                    ImGui::Text("Node cacheable: %s", node->uncacheable ? "false" : "true");
                    ImGui::Text("Node references: %lu", node->references.size());
                }
                ImGui::Text("Subscribers: %lu", signal.subscribers.size());
                for (auto &sub : signal.subscribers) {
                    auto subscriber = ecs::SignalRef(sub.lock());
                    if (subscriber) {
                        text = subscriber.String();
                        if (ImGui::Button(text.c_str())) {
                            this->selectedSignal = subscriber;
                        }
                    }
                }
                ImGui::Text("Dependencies: %lu", signal.dependencies.size());
                for (auto &dep : signal.dependencies) {
                    auto dependency = ecs::SignalRef(dep.lock());
                    if (dependency) {
                        text = dependency.String();
                        if (ImGui::Button(text.c_str())) {
                            this->selectedSignal = dependency;
                        }
                    }
                }
            }
        }
    }
} // namespace sp
