#include "Script.hh"
#include "assets/AssetManager.hh"
#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <cmath>
#include <glm/glm.hpp>
#include <robin_hood.h>
#include <sstream>

namespace ecs {
    static sp::CVar<std::string> CVarFlashlightParent("r.FlashlightParent",
        "player:flatview",
        "Flashlight parent entity name");

    robin_hood::unordered_node_map<std::string, OnTickFunc> ScriptDefinitions = {
        {"flashlight",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Light, TransformTree, SignalOutput, EventInput>(lock)) {
                    auto &light = ent.Get<Light>(lock);
                    auto &signalComp = ent.Get<SignalOutput>(lock);

                    light.on = signalComp.GetSignal("on") >= 0.5;
                    light.intensity = signalComp.GetSignal("intensity");
                    light.spotAngle = glm::radians(signalComp.GetSignal("angle"));

                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/flashlight/toggle", event)) {
                        signalComp.SetSignal("on", light.on ? 0.0 : 1.0);
                        light.on = !light.on;
                    }
                    while (EventInput::Poll(lock, ent, "/action/flashlight/grab", event)) {
                        auto &transform = ent.Get<TransformTree>(lock);
                        if (transform.parent.Has<TransformTree>(lock)) {
                            transform.pose = transform.GetGlobalTransform(lock);
                            transform.parent = Entity();
                        } else {
                            ecs::Name parentName;
                            if (parentName.Parse(CVarFlashlightParent.Get())) {
                                Entity parent = EntityWith<Name>(lock, parentName);
                                if (parent) {
                                    transform.pose.SetPosition(glm::vec3(0, -0.3, 0));
                                    transform.pose.SetRotation(glm::quat());
                                    transform.parent = parent;
                                } else {
                                    Errorf("Flashlight parent entity does not exist: %s", CVarFlashlightParent.Get());
                                }
                            } else {
                                Errorf("Flashlight parent entity name is invalid: %s", CVarFlashlightParent.Get());
                            }
                        }
                    }
                }
            }},
        {"sun",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<TransformTree, SignalOutput>(lock)) {
                    auto &transform = ent.Get<TransformTree>(lock);
                    auto &signalComp = ent.Get<SignalOutput>(lock);

                    auto sunPos = signalComp.GetSignal("position");
                    if (signalComp.GetSignal("fix_position") == 0.0) {
                        float intervalSeconds = interval.count() / 1e9;
                        sunPos += intervalSeconds * (0.05 + std::abs(sin(sunPos) * 0.1));
                        if (sunPos > M_PI_2) sunPos = -M_PI_2;
                        signalComp.SetSignal("position", sunPos);
                    }

                    transform.pose.SetRotation(glm::quat());
                    transform.pose.Rotate(glm::radians(-90.0), glm::vec3(1, 0, 0));
                    transform.pose.Rotate(sunPos, glm::vec3(0, 1, 0));
                    transform.pose.SetPosition(glm::vec3(sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0));
                }
            }},
        {"light_sensor",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<LightSensor, SignalOutput>(lock)) {
                    auto &sensorComp = ent.Get<LightSensor>(lock);
                    auto &outputComp = ent.Get<SignalOutput>(lock);

                    outputComp.SetSignal("light_value_r", sensorComp.illuminance.r);
                    outputComp.SetSignal("light_value_g", sensorComp.illuminance.g);
                    outputComp.SetSignal("light_value_b", sensorComp.illuminance.b);
                    auto triggerParam = state.GetParam<double>("trigger_level");
                    bool enabled = glm::all(
                        glm::greaterThanEqual(sensorComp.illuminance, glm::vec3(std::abs(triggerParam))));
                    if (triggerParam < 0) { enabled = !enabled; }
                    outputComp.SetSignal("value", enabled ? 1.0 : 0.0);

                    // add emissiveness to sensor when it is active
                    if (ent.Has<Renderable>(lock)) {
                        auto &renderable = ent.Get<Renderable>(lock);
                        if (triggerParam >= 0) {
                            renderable.emissive = enabled ? glm::vec3(0, 1, 0) : glm::vec3(0);
                        } else {
                            renderable.emissive = enabled ? glm::vec3(0) : glm::vec3(1, 0, 0);
                        }
                    }
                }
            }},
        {"joystick_calibration",
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
            }},
        {"auto_attach",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<TransformTree>(lock)) {
                    auto fullParentName = state.GetParam<std::string>("attach_parent");
                    auto scene = state.scene.lock();
                    ecs::Name parentName;
                    if (parentName.Parse(fullParentName, scene.get())) {
                        auto parentEntity = state.GetParam<NamedEntity>("attach_parent_entity");
                        if (parentEntity.Name() != parentName) parentEntity = NamedEntity(parentName);

                        auto &transform = ent.Get<TransformTree>(lock);
                        auto parent = parentEntity.Get(lock);
                        if (parent.Has<TransformTree>(lock)) {
                            if (ent.Has<Renderable>(lock)) {
                                auto &renderable = ent.Get<Renderable>(lock);
                                renderable.visibility.set();
                            }
                        } else {
                            parent = Entity();
                            if (ent.Has<Renderable>(lock)) {
                                auto &renderable = ent.Get<Renderable>(lock);
                                renderable.visibility.reset();
                            }
                        }
                        transform.parent = parent;
                        state.SetParam<NamedEntity>("attach_parent_entity", parentEntity);
                    } else {
                        Errorf("Attach parent name is invalid: %s", fullParentName);
                    }
                }
            }},
        {"lazy_load_model",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Renderable>(lock)) {
                    auto modelName = state.GetParam<std::string>("model_name");

                    auto &renderable = ent.Get<Renderable>(lock);
                    if (!renderable.model && sp::GAssets.IsGltfRegistered(modelName)) {
                        renderable.model = sp::GAssets.LoadGltf(modelName);
                        renderable.visibility.set();
                    }
                }
            }},
        {"relative_movement",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<SignalOutput>(lock)) {
                    auto fullTargetName = state.GetParam<std::string>("relative_to");
                    ecs::Name targetName;
                    auto scene = state.scene.lock();
                    if (targetName.Parse(fullTargetName, scene.get())) {
                        auto targetEntity = state.GetParam<NamedEntity>("target_entity");
                        if (targetEntity.Name() != targetName) targetEntity = NamedEntity(targetName);

                        auto target = targetEntity.Get(lock);
                        if (target) {
                            state.SetParam<NamedEntity>("target_entity", targetEntity);

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
            }},
        {"camera_view",
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
            }},
        {"model_spawner",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput>(lock)) {
                    auto scene = state.scene.lock();

                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/spawn", event)) {
                        glm::vec3 position;
                        position.x = state.GetParam<double>("position_x");
                        position.y = state.GetParam<double>("position_y");
                        position.z = state.GetParam<double>("position_z");
                        TransformTree transform(position);

                        auto fullTargetName = state.GetParam<std::string>("relative_to");
                        ecs::Name targetName;
                        if (targetName.Parse(fullTargetName, scene.get())) {
                            auto targetEntity = state.GetParam<NamedEntity>("target_entity");
                            if (targetEntity.Name() != targetName) targetEntity = NamedEntity(targetName);

                            auto target = targetEntity.Get(lock);
                            if (target) {
                                state.SetParam<NamedEntity>("target_entity", targetEntity);

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
            }},
        {"rotate",
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
            }},
        {"latch_signals",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<SignalOutput>(lock)) {
                    auto &signalOutput = ent.Get<SignalOutput>(lock);
                    for (auto &latchName : state.GetParam<std::vector<std::string>>("latches_names")) {
                        auto value = SignalBindings::GetSignal(lock, ent, latchName);
                        if (value >= 0.5) signalOutput.SetSignal(latchName, value);
                    }
                }
            }},
        {"grab_object",
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
            }},
    };
} // namespace ecs
