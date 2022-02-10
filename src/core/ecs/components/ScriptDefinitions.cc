#include "Script.hh"
#include "assets/AssetManager.hh"
#include "assets/Model.hh"
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
        "player.player",
        "Flashlight parent entity name");

    robin_hood::unordered_node_map<std::string, ScriptFunc> ScriptDefinitions = {
        {"flashlight",
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Light, TransformTree, SignalOutput, EventInput>(lock)) {
                    auto &light = ent.Get<Light>(lock);
                    auto &signalComp = ent.Get<SignalOutput>(lock);

                    light.on = signalComp.GetSignal("on") >= 0.5;
                    light.intensity = signalComp.GetSignal("intensity");
                    light.spotAngle = glm::radians(signalComp.GetSignal("angle"));

                    Event event;
                    if (EventInput::Poll(lock, ent, "/action/flashlight/toggle", event)) {
                        signalComp.SetSignal("on", light.on ? 0.0 : 1.0);
                        light.on = !light.on;
                    }
                    if (EventInput::Poll(lock, ent, "/action/flashlight/grab", event)) {
                        auto &transform = ent.Get<TransformTree>(lock);
                        if (transform.parent.Has<TransformTree>(lock)) {
                            transform.pose = transform.GetGlobalTransform(lock);
                            transform.parent = Tecs::Entity();
                        } else {
                            Tecs::Entity parent = EntityWith<Name>(lock, CVarFlashlightParent.Get());
                            if (parent) {
                                transform.pose.SetPosition(glm::vec3(0, -0.3, 0));
                                transform.pose.SetRotation(glm::quat());
                                transform.parent = parent;
                            } else {
                                Errorf("Flashlight parent entity does not exist: %s", CVarFlashlightParent.Get());
                            }
                        }
                    }
                }
            }},
        {"sun",
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
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
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Script, LightSensor, SignalOutput>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto &sensorComp = ent.Get<LightSensor>(lock);
                    auto &outputComp = ent.Get<SignalOutput>(lock);

                    outputComp.SetSignal("light_value_r", sensorComp.illuminance.r);
                    outputComp.SetSignal("light_value_g", sensorComp.illuminance.g);
                    outputComp.SetSignal("light_value_b", sensorComp.illuminance.b);
                    auto triggerParam = scriptComp.GetParam<double>("trigger_level");
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
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Name, Script, EventInput, EventBindings>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto &eventInput = ent.Get<EventInput>(lock);
                    auto &eventBindings = ent.Get<EventBindings>(lock);

                    Event event;
                    while (eventInput.Poll("/script/joystick_in", event)) {
                        auto data = std::get_if<glm::vec2>(&event.data);
                        if (data) {
                            float factorParamX = scriptComp.GetParam<double>("scale_x");
                            float factorParamY = scriptComp.GetParam<double>("scale_y");
                            eventBindings.SendEvent(lock,
                                "/script/joystick_out",
                                event.source,
                                glm::vec2(data->x * factorParamX, data->y * factorParamY));
                        } else {
                            Abortf("Unsupported event type: %s", event.toString());
                        }
                    }
                }
            }},
        {"auto_attach",
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Script, TransformTree>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto parentName = scriptComp.GetParam<std::string>("attach_parent");
                    auto parentEntity = scriptComp.GetParam<NamedEntity>("attach_parent_entity");
                    if (parentEntity.Name() != parentName) parentEntity = NamedEntity(parentName);

                    auto &transform = ent.Get<TransformTree>(lock);
                    auto parent = parentEntity.Get(lock);
                    if (parent.Has<TransformTree>(lock)) {
                        if (ent.Has<Renderable>(lock)) {
                            auto &renderable = ent.Get<Renderable>(lock);
                            renderable.visibility.set();
                        }
                    } else {
                        parent = Tecs::Entity();
                        if (ent.Has<Renderable>(lock)) {
                            auto &renderable = ent.Get<Renderable>(lock);
                            renderable.visibility.reset();
                        }
                    }
                    transform.parent = parent;
                    scriptComp.SetParam<NamedEntity>("attach_parent_entity", parentEntity);
                }
            }},
        {"lazy_load_model",
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Script, Renderable>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto modelName = scriptComp.GetParam<std::string>("model_name");

                    auto &renderable = ent.Get<Renderable>(lock);
                    if (!renderable.model || renderable.model->name != modelName) {
                        if (sp::GAssets.IsModelRegistered(modelName)) {
                            renderable.model = sp::GAssets.LoadModel(modelName);
                            renderable.visibility.set();
                        }
                    }
                }
            }},
        {"relative_movement",
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Script, SignalOutput>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto targetName = scriptComp.GetParam<std::string>("relative_to");
                    auto targetEntity = scriptComp.GetParam<NamedEntity>("target_entity");
                    if (targetEntity.Name() != targetName) targetEntity = NamedEntity(targetName);

                    auto target = targetEntity.Get(lock);
                    if (target) {
                        scriptComp.SetParam<NamedEntity>("target_entity", targetEntity);

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
                }
            }},
        {"camera_view",
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Script, EventInput, TransformTree>(lock)) {
                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/camera_rotate", event)) {
                        auto angleDiff = std::get<glm::vec2>(event.data);
                        if (SignalBindings::GetSignal(lock, ent, "interact_rotate") < 0.5) {
                            auto &scriptComp = ent.Get<Script>(lock);
                            auto sensitivity = scriptComp.GetParam<double>("view_sensitivity");

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
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Script, EventInput>(lock)) {
                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/spawn", event)) {
                        std::thread([ent]() {
                            auto lock = World.StartTransaction<AddRemove>();
                            if (ent.Has<Script, SceneInfo>(lock)) {
                                auto &scriptComp = ent.Get<Script>(lock);
                                auto &sceneInfo = ent.Get<SceneInfo>(lock);

                                auto newEntity = lock.NewEntity();
                                newEntity.Set<SceneInfo>(lock, newEntity, sceneInfo);

                                glm::vec3 position;
                                position.x = scriptComp.GetParam<double>("position_x");
                                position.y = scriptComp.GetParam<double>("position_y");
                                position.z = scriptComp.GetParam<double>("position_z");
                                auto &transform = newEntity.Set<TransformTree>(lock, position);

                                auto relativeName = scriptComp.GetParam<std::string>("relative_to");
                                if (!relativeName.empty()) {
                                    auto relative = EntityWith<Name>(lock, relativeName);
                                    if (relative.Has<TransformSnapshot>(lock)) {
                                        transform.pose.matrix = relative.Get<TransformSnapshot>(lock).matrix *
                                                                glm::mat4(transform.pose.matrix);
                                    }
                                }
                                newEntity.Set<TransformSnapshot>(lock, transform.pose);

                                auto modelName = scriptComp.GetParam<std::string>("model");
                                auto model = sp::GAssets.LoadModel(modelName);
                                newEntity.Set<Renderable>(lock, model);
                                newEntity.Set<Physics>(lock, model, PhysicsGroup::World, true);
                            }
                        }).detach();
                    }
                }
            }},
        {"rotate",
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Script, TransformTree>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    glm::vec3 rotationAxis;
                    rotationAxis.x = scriptComp.GetParam<double>("axis_x");
                    rotationAxis.y = scriptComp.GetParam<double>("axis_y");
                    rotationAxis.z = scriptComp.GetParam<double>("axis_z");
                    auto rotationSpeedRpm = scriptComp.GetParam<double>("speed");

                    auto &transform = ent.Get<TransformTree>(lock);
                    auto currentRotation = transform.pose.GetRotation();
                    transform.pose.SetRotation(glm::rotate(currentRotation,
                        (float)(rotationSpeedRpm * M_PI * 2.0 / 60.0 * interval.count() / 1e9),
                        rotationAxis));
                }
            }},
        {"latch_signals",
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Script, SignalOutput>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto &signalOutput = ent.Get<SignalOutput>(lock);
                    for (auto &latchName : scriptComp.GetParam<std::vector<std::string>>("latches_names")) {
                        auto value = SignalBindings::GetSignal(lock, ent, latchName);
                        if (value >= 0.5) signalOutput.SetSignal(latchName, value);
                    }
                }
            }},
        {"grab_object",
            [](Lock<WriteAll> lock, Tecs::Entity ent, chrono_clock::duration interval) {
                if (ent.Has<Script, EventInput, TransformSnapshot, PhysicsQuery>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto &query = ent.Get<PhysicsQuery>(lock);
                    auto &transform = ent.Get<TransformSnapshot>(lock);

                    auto target = scriptComp.GetParam<Tecs::Entity>("grab_target");
                    if (target.Has<Physics>(lock)) {
                        // Remove target if the constraint broke
                        auto &ph = target.Get<Physics>(lock);
                        if (ph.constraint != ent) {
                            ph.RemoveConstraint();
                            ph.group = PhysicsGroup::World;
                            target = Tecs::Entity();
                        }
                    }

                    Event event;
                    while (EventInput::Poll(lock, ent, "/action/interact_grab", event)) {
                        if (target.Has<Physics>(lock)) {
                            // Drop existing target entity
                            auto &ph = target.Get<Physics>(lock);
                            ph.RemoveConstraint();
                            ph.group = PhysicsGroup::World;
                            target = Tecs::Entity();
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
                    scriptComp.SetParam("grab_target", target);

                    auto inputSensitivity = (float)scriptComp.GetParam<double>("rotate_sensitivity");
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
                            auto center = ph.constraintRotation * ph.centerOfMass;
                            ph.constraintOffset += center - (deltaRotate * center);
                            ph.constraintRotation = deltaRotate * ph.constraintRotation;
                        }
                    }
                }
            }},
    };
} // namespace ecs
