/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "console/CVar.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalManager.hh"
#include "game/SceneManager.hh"
#include "input/BindingNames.hh"

#include <glm/gtx/quaternion.hpp>

namespace sp::scripts {
    using namespace ecs;

    static CVar<float> CVarEditRotateSensitivity("e.RotateSensitivity",
        2.0f,
        "Movement sensitivity for rotation in edit tool");
    static CVar<float> CVarEditRotateSnapDegrees("e.RotateSnapDegrees", 5.0f, "Snap angle for rotation in edit tool");
    static CVar<float> CVarEditTranslateSnap("e.TranslateSnap", 0.001f, "Snap distance for translation in edit tool");

    struct TraySpawner {
        std::string templateSource;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name != INTERACT_EVENT_INTERACT_GRAB) continue;

                // Ignore drop events
                if (!std::holds_alternative<Transform>(event.data)) continue;

                if (templateSource.empty()) {
                    Errorf("TraySpawner missing source parameter");
                    continue;
                }

                if (!ent.Has<Name, TransformTree>(lock)) continue;
                auto &sourceName = ent.Get<Name>(lock);
                auto transform = ent.Get<TransformTree>(lock).GetGlobalTransform(lock);

                std::optional<SignalOutput> signalOutputs;
                std::optional<SignalBindings> signalBindings;
                auto signals = GetSignalManager().GetSignals(ent);
                for (auto &signal : signals) {
                    SignalRef dstRef(ent, signal.GetSignalName());
                    if (signal.HasValue(lock)) {
                        if (!signalOutputs) signalOutputs = SignalOutput();
                        signalOutputs->signals[signal.GetSignalName()] = signal.GetValue(lock);
                    }
                    if (signal.HasBinding(lock)) {
                        if (!signalBindings) signalBindings = SignalBindings();
                        signalBindings->bindings[signal.GetSignalName()] = signal.GetBinding(lock);
                    }
                }

                SceneRef scene;
                if (lock.Has<ActiveScene>()) {
                    auto &active = lock.Get<ActiveScene>();
                    scene = active.scene;
                }
                if (!scene) {
                    Errorf("TraySpawner has no active scene");
                    continue;
                }

                auto sharedEntity = make_shared<EntityRef>();
                GetSceneManager().QueueAction(SceneAction::EditStagingScene,
                    scene.data->name,
                    [source = templateSource,
                        transform,
                        signalOutputs,
                        signalBindings,
                        baseName = sourceName.entity,
                        sharedEntity](ecs::Lock<ecs::AddRemove> lock, std::shared_ptr<Scene> scene) {
                        Name name(scene->data->name, "");
                        EntityScope scope = name;
                        for (size_t i = 0;; i++) {
                            name.entity = baseName + "_" + std::to_string(i);
                            if (!scene->GetStagingEntity(name)) break;
                        }
                        Logf("TraySpawner new entity: %s", name.String());

                        auto newEntity = scene->NewRootEntity(lock, scene, name);
                        newEntity.Set<TransformTree>(lock, transform);
                        if (signalOutputs) {
                            newEntity.Set<SignalOutput>(lock, *signalOutputs);
                        }
                        if (signalBindings) {
                            newEntity.Set<SignalBindings>(lock, *signalBindings);
                        }
                        auto &scripts = newEntity.Set<Scripts>(lock);
                        auto &prefab = scripts.AddPrefab(scope, "template");
                        prefab.SetParam("source", source);
                        ecs::GetScriptManager().RunPrefabs(lock, newEntity);

                        *sharedEntity = newEntity;
                    });
                GetSceneManager().QueueAction(SceneAction::ApplyStagingScene, scene.data->name);
                GetSceneManager().QueueAction([ent, target = event.source, sharedEntity] {
                    auto lock = ecs::StartTransaction<ecs::SendEventsLock>();
                    ecs::EventBindings::SendEvent(lock,
                        target,
                        ecs::Event{INTERACT_EVENT_INTERACT_GRAB, ent, sharedEntity->Get(lock)});
                });
            }
        }
    };
    StructMetadata MetadataTraySpawner(typeid(TraySpawner),
        "TraySpawner",
        "",
        StructField::New("source", &TraySpawner::templateSource));
    InternalScript<TraySpawner> traySpawner("tray_spawner", MetadataTraySpawner, true, INTERACT_EVENT_INTERACT_GRAB);

    struct EditTool {
        Entity selectedEntity;
        float toolDistance;
        glm::vec3 lastToolPosition = glm::vec3(0);
        glm::vec3 faceNormal = glm::vec3(0);
        PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;

        bool performUpdate(Lock<WriteAll> lock, float toolDepth, int editMode, bool snapToFace) {
            auto deltaDepth = toolDepth;
            if (!snapToFace) {
                deltaDepth = std::round(toolDepth / CVarEditTranslateSnap.Get()) * CVarEditTranslateSnap.Get();
            }
            auto deltaVector = deltaDepth * faceNormal;

            auto &targetTree = selectedEntity.Get<TransformTree>(lock);
            auto parent = targetTree.parent.Get(lock);
            Transform parentTransform;
            if (parent.Has<TransformTree>(lock)) {
                parentTransform = parent.Get<const TransformTree>(lock).GetGlobalTransform(lock);
                deltaVector = parentTransform.GetInverse() * glm::vec4(deltaVector, 0.0f);
            }

            if (editMode == 0) { // Translate mode
                if (deltaDepth == 0.0f) return false;
                targetTree.pose.Translate(deltaVector);
            } else if (editMode == 1) { // Scale mode
                if (deltaDepth == 0.0f) return false;
                // Simulate an extrude tool by anchoring the opposite face (assuming model is symmetric around origin)
                // Calculate the required scale to move the face by the tool depth in world space
                auto targetTransform = targetTree.GetGlobalTransform(lock);
                auto relativeNormal = targetTransform.GetInverse() * glm::vec4(faceNormal, 0.0f);
                auto scaleFactor = 1.0f + glm::abs(relativeNormal) * deltaDepth;

                // Make sure we don't invert the scale
                if (glm::any(glm::lessThanEqual(scaleFactor, glm::vec3(0.0f)))) return false;

                targetTree.pose.SetScale(
                    glm::clamp(targetTree.pose.GetScale() * scaleFactor, glm::vec3(1e-4), glm::vec3(1e6)));
                targetTree.pose.Translate(deltaVector * 0.5f);
            } else if (editMode == 2) { // Rotate mode
                auto targetTransform = targetTree.GetGlobalTransform(lock);
                auto relativeNormal = targetTransform.GetInverse() * glm::vec4(faceNormal, 0.0f);

                auto snapRadians = glm::radians(CVarEditRotateSnapDegrees.Get());
                auto angle = std::round(toolDepth * CVarEditRotateSensitivity.Get() / snapRadians) * snapRadians;
                if (angle == 0.0f) return false;
                targetTree.pose.Rotate(angle, relativeNormal);
            }
            selectedEntity.Set<TransformSnapshot>(lock, parentTransform * targetTree.pose);
            return true;
        }

        void performRotateToFace(Lock<WriteAll> lock, glm::vec3 targetNormal) {
            auto &targetTree = selectedEntity.Get<TransformTree>(lock);
            auto worldToLocalRotation = glm::inverse(targetTree.GetGlobalRotation(lock));
            auto deltaRotation = glm::rotation(worldToLocalRotation * faceNormal, worldToLocalRotation * -targetNormal);
            targetTree.pose.Rotate(deltaRotation);
            selectedEntity.Set<TransformSnapshot>(lock, targetTree.GetGlobalTransform(lock));
        }

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformTree, PhysicsQuery>(lock)) return;
            auto &query = ent.Get<PhysicsQuery>(lock);
            auto &transform = ent.Get<TransformTree>(lock);

            PhysicsQuery::Raycast::Result raycastResult = {};
            if (!raycastQuery) {
                raycastQuery = query.NewQuery(PhysicsQuery::Raycast(100.0f,
                    PhysicsGroupMask(PHYSICS_GROUP_WORLD | PHYSICS_GROUP_INTERACTIVE | PHYSICS_GROUP_USER_INTERFACE)));
            } else {
                auto &result = query.Lookup(raycastQuery).result;
                if (result) raycastResult = result.value();
            }

            auto globalTransform = transform.GetGlobalTransform(lock);
            auto position = globalTransform.GetPosition();
            auto forward = globalTransform.GetForward();

            SignalRef editModeRef(ent, "edit_mode");
            auto editMode = (int)editModeRef.GetSignal(lock);
            editMode = std::clamp(editMode, 0, 2);
            editModeRef.SetValue(lock, editMode);
            auto snapMode = SignalRef(ent, "snap_mode").GetSignal(lock) >= 0.5;

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name == INTERACT_EVENT_INTERACT_PRESS) {
                    if (std::holds_alternative<bool>(event.data)) {
                        if (std::get<bool>(event.data) && raycastResult.subTarget.Has<TransformTree>(lock)) {
                            selectedEntity = raycastResult.subTarget;
                            toolDistance = raycastResult.distance;
                            lastToolPosition = position + forward * toolDistance;
                            if (raycastResult.normal != glm::vec3(0)) {
                                faceNormal = glm::normalize(raycastResult.normal);
                            } else {
                                faceNormal = glm::vec3(0);
                            }
                        } else if (selectedEntity) {
                            if (snapMode && raycastResult.subTarget) {
                                if (editMode == 2) {
                                    performRotateToFace(lock, raycastResult.normal);
                                } else {
                                    // Project the tool position onto the face normal to get a depth scalar
                                    auto newToolPosition = position + forward * raycastResult.distance;
                                    auto projectedDepth = glm::dot(newToolPosition - lastToolPosition, faceNormal);
                                    performUpdate(lock, projectedDepth, editMode, true);
                                }
                            }
                            selectedEntity = {};
                        }
                    }
                }
            }

            float cursorLength = 0.1f;
            if (snapMode && selectedEntity && raycastResult.subTarget && editMode != 2) {
                auto newToolPosition = position + forward * raycastResult.distance;
                cursorLength = glm::dot(newToolPosition - lastToolPosition, faceNormal);
            }

            if (ent.Has<LaserLine>(lock)) {
                auto &laserLine = ent.Get<LaserLine>(lock);
                if (selectedEntity || raycastResult.subTarget) {
                    laserLine.on = true;
                    laserLine.mediaDensityFactor = 0.0f;
                    laserLine.relative = false;
                    LaserLine::Line line;
                    if (editMode == 0) {
                        line.color = glm::vec3(0, 0, 1);
                    } else if (editMode == 1) {
                        line.color = glm::vec3(0, 1, 0);
                    } else if (editMode == 2) {
                        line.color = glm::vec3(1, 0, 0);
                    } else {
                        line.color = glm::vec3(1, 1, 1);
                    }
                    if (selectedEntity) {
                        laserLine.intensity = 10.0f;
                        line.points = {lastToolPosition, lastToolPosition + faceNormal * cursorLength};
                    } else {
                        laserLine.intensity = 1.0f;
                        auto cursorPosition = position + forward * raycastResult.distance;
                        line.points = {cursorPosition, cursorPosition + raycastResult.normal * cursorLength};
                    }
                    laserLine.line = line;
                } else {
                    laserLine.on = false;
                }
            }

            if (!snapMode && selectedEntity.Has<TransformTree>(lock) && faceNormal != glm::vec3(0)) {
                // Project the tool position onto the face normal to get a depth scalar
                auto newToolPosition = position + forward * toolDistance;
                auto projectedDepth = glm::dot(newToolPosition - lastToolPosition, faceNormal);
                if (performUpdate(lock, projectedDepth, editMode, false)) {
                    lastToolPosition += projectedDepth * faceNormal;
                }
            }
        }
    };
    StructMetadata MetadataEditTool(typeid(EditTool), "EditTool", "");
    InternalScript<EditTool> editTool("edit_tool", MetadataEditTool, false, INTERACT_EVENT_INTERACT_PRESS);
} // namespace sp::scripts
