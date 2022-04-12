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
        InternalScript("joystick_calibration",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Name, EventInput, EventBindings>(lock)) {
                    auto &eventBindings = ent.Get<EventBindings>(lock);

                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/joystick_in", event)) {
                        auto data = std::get_if<glm::vec2>(&event.data);
                        if (data) {
                            float factorParamX = state.GetParam<double>("scale_x");
                            float factorParamY = state.GetParam<double>("scale_y");
                            eventBindings.SendEvent(lock,
                                "/script/joystick_out",
                                event.source,
                                glm::vec2(data->x * factorParamX, data->y * factorParamY));
                        } else {
                            Errorf("Unsupported joystick_in event type: %s", event.toString());
                        }
                    }
                }
            }),
        InternalScript("relative_movement",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<SignalOutput>(lock)) {
                    auto fullTargetName = state.GetParam<std::string>("relative_to");
                    ecs::Name targetName;
                    if (targetName.Parse(fullTargetName, state.scope.prefix)) {
                        auto targetEntity = state.GetParam<EntityRef>("target_entity");
                        if (targetEntity.Name() != targetName) targetEntity = targetName;

                        auto target = targetEntity.Get();
                        if (target) {
                            state.SetParam<EntityRef>("target_entity", targetEntity);

                            glm::vec3 movement = glm::vec3(0);
                            movement.z -= SignalBindings::GetSignal(lock, ent, "move_forward");
                            movement.z += SignalBindings::GetSignal(lock, ent, "move_back");
                            movement.x -= SignalBindings::GetSignal(lock, ent, "move_left");
                            movement.x += SignalBindings::GetSignal(lock, ent, "move_right");
                            float vertical = SignalBindings::GetSignal(lock, ent, "move_up");
                            vertical -= SignalBindings::GetSignal(lock, ent, "move_down");

                            movement.x = std::clamp(movement.x, -1.0f, 1.0f);
                            movement.z = std::clamp(movement.z, -1.0f, 1.0f);
                            vertical = std::clamp(vertical, -1.0f, 1.0f);

                            if (target.Has<TransformTree>(lock)) {
                                auto parentRotation = target.Get<const TransformTree>(lock).GetGlobalRotation(lock);
                                movement = parentRotation * movement;
                                if (std::abs(movement.y) > 0.999) {
                                    movement = parentRotation * glm::vec3(0, -movement.y, 0);
                                }
                                movement.y = 0;
                            }

                            auto &outputComp = ent.Get<SignalOutput>(lock);
                            outputComp.SetSignal("move_world_x", movement.x);
                            outputComp.SetSignal("move_world_y", vertical);
                            outputComp.SetSignal("move_world_z", movement.z);
                        }
                    } else {
                        Errorf("Relative target name is invalid: %s", fullTargetName);
                    }
                }
            }),
        InternalScript("camera_view",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput, TransformTree>(lock)) {
                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/camera_rotate", event)) {
                        auto angleDiff = std::get<glm::vec2>(event.data);
                        if (SignalBindings::GetSignal(lock, ent, "interact_rotate") < 0.5) {
                            auto sensitivity = state.GetParam<double>("view_sensitivity");

                            // Apply pitch/yaw rotations
                            auto &transform = ent.Get<TransformTree>(lock);
                            auto rotation = glm::quat(glm::vec3(0, -angleDiff.x * sensitivity, 0)) *
                                            transform.pose.GetRotation() *
                                            glm::quat(glm::vec3(-angleDiff.y * sensitivity, 0, 0));

                            auto up = rotation * glm::vec3(0, 1, 0);
                            if (up.y < 0) {
                                // Camera is turning upside-down, reset it
                                auto right = rotation * glm::vec3(1, 0, 0);
                                right.y = 0;
                                up.y = 0;
                                glm::vec3 forward = glm::cross(right, up);
                                rotation = glm::quat_cast(
                                    glm::mat3(glm::normalize(right), glm::normalize(up), glm::normalize(forward)));
                            }
                            transform.pose.SetRotation(rotation);
                        }
                    }
                }
            }),
        InternalScript("model_spawner",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput>(lock)) {
                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/spawn", event)) {
                        glm::vec3 position;
                        position.x = state.GetParam<double>("position_x");
                        position.y = state.GetParam<double>("position_y");
                        position.z = state.GetParam<double>("position_z");
                        TransformTree transform(position);

                        auto fullTargetName = state.GetParam<std::string>("relative_to");
                        ecs::Name targetName;
                        if (targetName.Parse(fullTargetName, state.scope.prefix)) {
                            auto targetEntity = state.GetParam<EntityRef>("target_entity");
                            if (targetEntity.Name() != targetName) targetEntity = targetName;

                            auto target = targetEntity.Get();
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

                        std::thread([ent, transform, model]() {
                            auto lock = World.StartTransaction<AddRemove>();
                            if (ent.Has<SceneInfo>(lock)) {
                                auto &sceneInfo = ent.Get<SceneInfo>(lock);

                                auto newEntity = lock.NewEntity();
                                newEntity.Set<SceneInfo>(lock, newEntity, sceneInfo);

                                Component<TransformTree>::Apply(transform, lock, newEntity);

                                newEntity.Set<Renderable>(lock, model);
                                newEntity.Set<Physics>(lock, model);
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
        InternalScript("latch_signals",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<SignalOutput>(lock)) {
                    auto &signalOutput = ent.Get<SignalOutput>(lock);
                    for (auto &latchName : state.GetParam<std::vector<std::string>>("latches_names")) {
                        auto value = SignalBindings::GetSignal(lock, ent, latchName);
                        if (value >= 0.5) signalOutput.SetSignal(latchName, value);
                    }
                }
            }),
        InternalScript("grab_object",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput, TransformSnapshot, PhysicsQuery>(lock)) {
                    auto &query = ent.Get<PhysicsQuery>(lock);
                    auto &transform = ent.Get<TransformSnapshot>(lock);

                    auto &target = query.centerOfMassQuery;
                    if (target.Has<Physics>(lock)) {
                        // Remove target if the constraint broke
                        auto &ph = target.Get<Physics>(lock);
                        if (ph.constraint != ent) {
                            ph.RemoveConstraint();
                            ph.group = PhysicsGroup::World;
                            if (target.Has<Renderable>(lock)) {
                                target.Get<Renderable>(lock).visibility.reset(
                                    ecs::Renderable::Visibility::VISIBLE_OUTLINE_SELECTION);
                            }
                            target = Entity();
                        }
                    }

                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/interact_grab", event)) {
                        if (target.Has<Physics>(lock)) {
                            // Drop existing target entity
                            auto &ph = target.Get<Physics>(lock);
                            ph.RemoveConstraint();
                            ph.group = PhysicsGroup::World;
                            if (target.Has<Renderable>(lock)) {
                                target.Get<Renderable>(lock).visibility.reset(
                                    ecs::Renderable::Visibility::VISIBLE_OUTLINE_SELECTION);
                            }
                            target = Entity();
                        } else if (query.raycastHitTarget.Has<Physics, TransformSnapshot>(lock)) {
                            // Grab the entity being looked at
                            auto &ph = query.raycastHitTarget.Get<Physics>(lock);
                            if (ph.dynamic && !ph.kinematic && !ph.constraint) {
                                target = query.raycastHitTarget;

                                auto &hitTransform = target.Get<TransformSnapshot>(lock);
                                auto invParentRotate = glm::inverse(transform.GetRotation());

                                ph.group = PhysicsGroup::PlayerHands;
                                ph.SetConstraint(ent,
                                    query.raycastQueryDistance,
                                    invParentRotate *
                                        (hitTransform.GetPosition() - transform.GetPosition() + glm::vec3(0, 0.1, 0)),
                                    invParentRotate * hitTransform.GetRotation());
                            }
                        }
                    }

                    if (state.userData.has_value()) {
                        auto lastSelection = std::any_cast<Entity>(state.userData);
                        if (lastSelection && lastSelection.Has<Renderable>(lock)) {
                            lastSelection.Get<Renderable>(lock).visibility.reset(
                                ecs::Renderable::Visibility::VISIBLE_OUTLINE_SELECTION);
                        }
                    }

                    auto selection = target;
                    if (!selection) selection = query.raycastHitTarget;

                    if (selection && selection.Has<Renderable>(lock)) {
                        selection.Get<Renderable>(lock).visibility.set(
                            ecs::Renderable::Visibility::VISIBLE_OUTLINE_SELECTION);
                        state.userData = selection;
                    }

                    auto inputSensitivity = (float)state.GetParam<double>("rotate_sensitivity");
                    if (inputSensitivity == 0.0f) inputSensitivity = 0.001f;
                    bool rotating = SignalBindings::GetSignal(lock, ent, "interact_rotate") >= 0.5;
                    while (EventInput::Poll(lock, ent, "/action/interact_rotate", event)) {
                        if (rotating && target.Has<Physics>(lock)) {
                            auto input = std::get<glm::vec2>(event.data) * inputSensitivity;
                            auto upAxis = glm::inverse(transform.GetRotation()) * glm::vec3(0, 1, 0);
                            auto deltaRotate = glm::angleAxis(input.y, glm::vec3(1, 0, 0)) *
                                               glm::angleAxis(input.x, upAxis);

                            // Move the objects origin so it rotates around its center of mass
                            auto &ph = target.Get<Physics>(lock);
                            auto center = ph.constraintRotation * query.centerOfMass;
                            ph.constraintOffset += center - (deltaRotate * center);
                            ph.constraintRotation = deltaRotate * ph.constraintRotation;
                        }
                    }
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

                    auto targetPosition = glm::vec3(0);
                    auto fullTargetName = state.GetParam<std::string>("follow_target");
                    ecs::Name targetName;
                    if (targetName.Parse(fullTargetName, state.scope.prefix)) {
                        auto targetEntity = state.GetParam<EntityRef>("target_entity");
                        if (targetEntity.Name() != targetName) targetEntity = targetName;

                        auto target = targetEntity.Get();
                        if (target) {
                            state.SetParam<EntityRef>("target_entity", targetEntity);

                            if (target.Has<TransformSnapshot>(lock)) {
                                targetPosition = target.Get<TransformSnapshot>(lock).GetPosition();
                            }
                        }
                    }

                    targetPosition = voxelRotation * targetPosition + voxelOffset;
                    targetPosition = glm::floor(targetPosition / voxelStride / voxelScale) * voxelScale * voxelStride;
                    transform.pose.SetPosition(glm::inverse(voxelRotation) * targetPosition);
                    transform.pose.SetScale(glm::vec3(voxelScale));
                }
            }),
    };
} // namespace sp::scripts
