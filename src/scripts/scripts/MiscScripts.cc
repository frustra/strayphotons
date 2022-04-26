#include "assets/AssetManager.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityReferenceManager.hh"
#include "game/Scene.hh"

#include <cmath>
#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    std::array miscScripts = {
        InternalScript("edge_trigger",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                auto inputName = state.GetParam<std::string>("input_signal");
                auto outputName = state.GetParam<std::string>("output_event");
                auto upperThreshold = state.GetParam<double>("upper_threshold");
                auto lowerThreshold = state.GetParam<double>("lower_threshold");

                auto oldTriggered = state.GetParam<bool>("triggered");
                auto newTriggered = oldTriggered;
                auto value = SignalBindings::GetSignal(lock, ent, inputName);
                if (upperThreshold >= lowerThreshold) {
                    if (value >= upperThreshold && !oldTriggered) {
                        newTriggered = true;
                    } else if (value <= lowerThreshold && oldTriggered) {
                        newTriggered = false;
                    }
                } else {
                    if (value <= upperThreshold && !oldTriggered) {
                        newTriggered = true;
                    } else if (value >= lowerThreshold && oldTriggered) {
                        newTriggered = false;
                    }
                }
                if (newTriggered != oldTriggered) {
                    if (newTriggered != 0.0f) EventBindings::SendEvent(lock, outputName, ent, (double)newTriggered);
                    state.SetParam<bool>("triggered", newTriggered);
                }
            }),
        InternalScript("model_spawner",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput>(lock)) {
                    Event event;
                    while (EventInput::Poll(lock, ent, "/script/spawn", event)) {
                        glm::vec3 position;
                        position.x = state.GetParam<double>("position_x");
                        position.y = state.GetParam<double>("position_y");
                        position.z = state.GetParam<double>("position_z");
                        TransformTree transform(position);

                        auto fullTargetName = state.GetParam<std::string>("relative_to");
                        ecs::Name targetName(fullTargetName, state.scope.prefix);
                        if (targetName) {
                            auto targetEntity = state.GetParam<EntityRef>("target_entity");
                            if (targetEntity.Name() != targetName) targetEntity = targetName;

                            auto target = targetEntity.Get(lock);
                            if (target) {
                                state.SetParam<EntityRef>("target_entity", targetEntity);

                                if (target.Has<TransformSnapshot>(lock)) {
                                    transform.pose.matrix = target.Get<TransformSnapshot>(lock).matrix *
                                                            glm::mat4(transform.pose.matrix);
                                }
                            }
                        }

                        auto modelName = state.GetParam<std::string>("model");
                        auto model = sp::GAssets.LoadGltf(modelName);

                        std::thread([ent, transform, model, scope = state.scope]() {
                            auto lock = World.StartTransaction<AddRemove>();
                            if (ent.Has<SceneInfo>(lock)) {
                                auto &sceneInfo = ent.Get<SceneInfo>(lock);

                                auto newEntity = lock.NewEntity();
                                newEntity.Set<SceneInfo>(lock, newEntity, sceneInfo);

                                Component<TransformTree>::Apply(transform, lock, newEntity);

                                newEntity.Set<Renderable>(lock, model);
                                newEntity.Set<Physics>(lock, model);
                                newEntity.Set<PhysicsJoints>(lock);
                                newEntity.Set<PhysicsQuery>(lock);
                                newEntity.Set<EventInput>(lock,
                                    "/interact/grab",
                                    "/interact/point",
                                    "/interact/rotate",
                                    "/physics/broken_constraint");
                                auto &script = newEntity.Set<Script>(lock);
                                auto &newState = script.AddOnTick(scope, ScriptDefinitions.at("interactive_object"));
                                newState.filterEvents = {
                                    "/interact/grab",
                                    "/interact/point",
                                    "/interact/rotate",
                                    "/physics/broken_constraint",
                                };
                            }
                        }).detach();
                    }
                }
            }),
        InternalScript("rotate",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<TransformTree>(lock)) {
                    glm::vec3 rotationAxis;
                    rotationAxis.x = state.GetParam<double>("axis_x");
                    rotationAxis.y = state.GetParam<double>("axis_y");
                    rotationAxis.z = state.GetParam<double>("axis_z");
                    auto rotationSpeedRpm = state.GetParam<double>("speed");

                    auto &transform = ent.Get<TransformTree>(lock);
                    auto currentRotation = transform.pose.GetRotation();
                    transform.pose.SetRotation(glm::rotate(currentRotation,
                        (float)(rotationSpeedRpm * M_PI * 2.0 / 60.0 * interval.count() / 1e9),
                        rotationAxis));
                }
            }),
        InternalScript("interactive_object",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput, TransformSnapshot, Physics, PhysicsJoints>(lock)) {
                    auto &ph = ent.Get<Physics>(lock);
                    auto &joints = ent.Get<PhysicsJoints>(lock);
                    Assertf(ph.dynamic && !ph.kinematic,
                        "Interactive object must have dynamic physics: %s",
                        ToString(lock, ent));

                    struct ScriptData {
                        Entity grabEntity, pointEntity;
                        Transform pointTransform;
                    };

                    ScriptData scriptData;
                    if (state.userData.has_value()) scriptData = std::any_cast<ScriptData>(state.userData);

                    glm::vec3 centerOfMass;
                    if (ent.Has<PhysicsQuery>(lock)) {
                        auto &query = ent.Get<PhysicsQuery>(lock);
                        if (!query.centerOfMassQuery) {
                            query.centerOfMassQuery = ent;
                        } else if (query.centerOfMassQuery != ent) {
                            Abortf("PhysicsQuery is pointing to wrong entity for interactive_object: %s",
                                ToString(lock, ent));
                        } else {
                            centerOfMass = query.centerOfMass;
                        }
                    }

                    Event event;
                    while (EventInput::Poll(lock, ent, PHYSICS_EVENT_BROKEN_CONSTRAINT, event)) {
                        ph.RemoveConstraint();
                        ph.group = PhysicsGroup::World;
                        scriptData.grabEntity = {};
                    }

                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_POINT, event)) {
                        auto pointTransform = std::get_if<Transform>(&event.data);
                        if (pointTransform) {
                            scriptData.pointTransform = *pointTransform;
                            scriptData.pointEntity = event.source;
                        } else if (std::holds_alternative<bool>(event.data)) {
                            scriptData.pointTransform = {};
                            scriptData.pointEntity = {};
                        } else {
                            Errorf("Unsupported point event type: %s", event.toString());
                        }
                    }

                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_GRAB, event)) {
                        if (std::holds_alternative<bool>(event.data)) {
                            // Grab(false) = Drop
                            ph.RemoveConstraint();
                            joints.joints.erase(std::remove_if(joints.joints.begin(),
                                                    joints.joints.end(),
                                                    [&](auto &&joint) {
                                                        return joint.target == scriptData.grabEntity &&
                                                               joint.type == PhysicsJointType::Fixed;
                                                    }),
                                joints.joints.end());
                            ph.group = PhysicsGroup::World;
                            scriptData.grabEntity = {};
                        } else if (std::holds_alternative<Transform>(event.data)) {
                            auto &parentTransform = std::get<Transform>(event.data);
                            auto &transform = ent.Get<TransformSnapshot>(lock);
                            auto invParentRotate = glm::inverse(parentTransform.GetRotation());

                            scriptData.grabEntity = event.source;
                            ph.group = PhysicsGroup::PlayerHands;

                            if (event.source.Has<Physics>(lock)) {
                                PhysicsJoint joint;
                                joint.target = event.source;
                                joint.type = PhysicsJointType::Fixed;
                                joint.remoteOffset = invParentRotate *
                                                     (transform.GetPosition() - parentTransform.GetPosition());
                                joint.remoteOrient = invParentRotate * transform.GetRotation();
                                joints.Add(joint);
                            } else {
                                ph.SetConstraint(event.source,
                                    0.0f,
                                    invParentRotate * (transform.GetPosition() - parentTransform.GetPosition()),
                                    invParentRotate * transform.GetRotation());
                            }
                        } else {
                            Errorf("Unsupported grab event type: %s", event.toString());
                        }
                    }

                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_ROTATE, event)) {
                        if (std::holds_alternative<glm::vec2>(event.data)) {
                            if (!scriptData.grabEntity.Has<TransformSnapshot>(lock)) continue;

                            auto &input = std::get<glm::vec2>(event.data);
                            auto &transform = scriptData.grabEntity.Get<const TransformSnapshot>(lock);

                            auto upAxis = glm::inverse(transform.GetRotation()) * glm::vec3(0, 1, 0);
                            auto deltaRotate = glm::angleAxis(input.y, glm::vec3(1, 0, 0)) *
                                               glm::angleAxis(input.x, upAxis);

                            for (auto &joint : joints.joints) {
                                if (joint.target == scriptData.grabEntity && joint.type == PhysicsJointType::Fixed) {
                                    // Move the objects origin so it rotates around its center of mass
                                    auto center = joint.remoteOrient * centerOfMass;
                                    joint.remoteOffset += center - (deltaRotate * center);
                                    joint.remoteOrient = deltaRotate * joint.remoteOrient;
                                    break;
                                }
                            }
                            if (ph.constraint) {
                                // Move the objects origin so it rotates around its center of mass
                                auto center = ph.constraintRotation * centerOfMass;
                                ph.constraintOffset += center - (deltaRotate * center);
                                ph.constraintRotation = deltaRotate * ph.constraintRotation;
                            }
                        }
                    }

                    if (ent.Has<Renderable>(lock)) {
                        ent.Get<Renderable>(lock).visibility.set(Renderable::Visibility::VISIBLE_OUTLINE_SELECTION,
                            scriptData.grabEntity || scriptData.pointEntity);
                    }

                    state.userData = scriptData;
                }
            }),
        InternalScript("interact_handler",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput, TransformSnapshot, PhysicsQuery>(lock)) {
                    auto &query = ent.Get<PhysicsQuery>(lock);
                    auto &transform = ent.Get<TransformSnapshot>(lock);

                    struct ScriptData {
                        Entity grabEntity, pointEntity;
                    };

                    ScriptData scriptData;
                    if (state.userData.has_value()) scriptData = std::any_cast<ScriptData>(state.userData);

                    Event event;
                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_GRAB, event)) {
                        if (std::holds_alternative<bool>(event.data)) {
                            auto &grabEvent = std::get<bool>(event.data);
                            if (scriptData.grabEntity) {
                                // Drop the currently held entity
                                EventBindings::SendEvent(lock,
                                    scriptData.grabEntity,
                                    INTERACT_EVENT_INTERACT_GRAB,
                                    Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                                scriptData.grabEntity = {};
                            }
                            if (grabEvent && query.raycastHitTarget) {
                                // Grab the entity being looked at
                                if (EventBindings::SendEvent(lock,
                                        query.raycastHitTarget,
                                        INTERACT_EVENT_INTERACT_GRAB,
                                        Event{INTERACT_EVENT_INTERACT_GRAB, ent, transform}) > 0) {
                                    scriptData.grabEntity = query.raycastHitTarget;
                                }
                            }
                        } else {
                            Errorf("Unsupported grab event type: %s", event.toString());
                        }
                    }

                    bool rotating = SignalBindings::GetSignal(lock, ent, "interact_rotate") >= 0.5;
                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_ROTATE, event)) {
                        if (rotating && scriptData.grabEntity) {
                            ecs::EventBindings::SendEvent(lock,
                                scriptData.grabEntity,
                                INTERACT_EVENT_INTERACT_ROTATE,
                                Event{INTERACT_EVENT_INTERACT_ROTATE, ent, event.data});
                        }
                    }

                    if (scriptData.pointEntity && query.raycastHitTarget != scriptData.pointEntity) {
                        EventBindings::SendEvent(lock,
                            scriptData.pointEntity,
                            INTERACT_EVENT_INTERACT_POINT,
                            Event{INTERACT_EVENT_INTERACT_POINT, ent, false});
                    } else if (query.raycastHitTarget) {
                        ecs::Transform pointTransfrom = transform;
                        pointTransfrom.SetPosition(query.raycastHitPosition);
                        EventBindings::SendEvent(lock,
                            query.raycastHitTarget,
                            INTERACT_EVENT_INTERACT_POINT,
                            Event{INTERACT_EVENT_INTERACT_POINT, ent, pointTransfrom});
                    }
                    scriptData.pointEntity = query.raycastHitTarget;

                    state.userData = scriptData;
                }
            }),
        InternalScript("voxel_controller",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<TransformTree, VoxelArea>(lock)) {
                    auto &transform = ent.Get<TransformTree>(lock);
                    auto &voxelArea = ent.Get<VoxelArea>(lock);
                    auto voxelRotation = transform.GetGlobalRotation(lock);

                    auto voxelScale = (float)state.GetParam<double>("voxel_scale");
                    auto voxelStride = std::max(1.0f, (float)state.GetParam<double>("voxel_stride"));
                    glm::vec3 voxelOffset;
                    voxelOffset.x = state.GetParam<double>("voxel_offset_x");
                    voxelOffset.y = state.GetParam<double>("voxel_offset_y");
                    voxelOffset.z = state.GetParam<double>("voxel_offset_z");
                    voxelOffset *= glm::vec3(voxelArea.extents) * voxelScale;

                    ecs::Name alignmentName(state.GetParam<std::string>("alignment_target"), state.scope.prefix);
                    if (alignmentName) {
                        auto alignmentEntity = state.GetParam<EntityRef>("alignment_entity");
                        bool existingAlignment = (bool)alignmentEntity;
                        if (alignmentEntity.Name() != alignmentName) alignmentEntity = alignmentName;

                        auto previousAlignment = state.GetParam<glm::vec3>("previous_alignment");
                        glm::vec3 alignmentOffset;
                        auto target = alignmentEntity.Get(lock);
                        if (target) {
                            state.SetParam<EntityRef>("alignment_entity", alignmentEntity);

                            if (target.Has<TransformSnapshot>(lock)) {
                                if (existingAlignment) {
                                    alignmentOffset = target.Get<TransformSnapshot>(lock).GetPosition() -
                                                      previousAlignment;
                                } else {
                                    state.SetParam<glm::vec3>("previous_alignment",
                                        target.Get<TransformSnapshot>(lock).GetPosition());
                                }
                            } else {
                                state.SetParam<EntityRef>("alignment_entity", EntityRef());
                            }
                        } else {
                            state.SetParam<EntityRef>("alignment_entity", EntityRef());
                        }
                        voxelOffset += glm::mod(alignmentOffset, voxelStride * voxelScale);
                    }

                    auto targetPosition = glm::vec3(0);
                    ecs::Name targetName(state.GetParam<std::string>("follow_target"), state.scope.prefix);
                    if (targetName) {
                        auto followEntity = state.GetParam<EntityRef>("follow_entity");
                        if (followEntity.Name() != targetName) followEntity = targetName;

                        auto target = followEntity.Get(lock);
                        if (target) {
                            state.SetParam<EntityRef>("follow_entity", followEntity);

                            if (target.Has<TransformSnapshot>(lock)) {
                                targetPosition = target.Get<TransformSnapshot>(lock).GetPosition();
                            }
                        }
                    }

                    targetPosition = voxelRotation * targetPosition;
                    targetPosition = glm::floor(targetPosition / voxelStride / voxelScale) * voxelScale * voxelStride;
                    transform.pose.SetPosition(glm::inverse(voxelRotation) * (targetPosition + voxelOffset));
                    transform.pose.SetScale(glm::vec3(voxelScale));
                }
            }),
    };
} // namespace sp::scripts
