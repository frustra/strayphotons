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
        "player.player",
        "Flashlight parent entity name");

    robin_hood::unordered_node_map<std::string, ScriptFunc> ScriptDefinitions = {
        {"flashlight",
            [](Lock<WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
                if (ent.Has<Light, Transform, SignalOutput, EventInput>(lock)) {
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
                        auto &transform = ent.Get<Transform>(lock);
                        if (transform.HasParent(lock)) {
                            transform.SetPosition(transform.GetGlobalPosition(lock));
                            transform.SetRotation(transform.GetGlobalRotation(lock));
                            transform.SetParent(Tecs::Entity());
                        } else {
                            Tecs::Entity parent = EntityWith<Name>(lock, CVarFlashlightParent.Get());
                            if (parent) {
                                transform.SetPosition(glm::vec3(0, -0.3, 0));
                                transform.SetRotation(glm::quat());
                                transform.SetParent(parent);
                            } else {
                                Errorf("Flashlight parent entity does not exist: %s", CVarFlashlightParent.Get());
                            }
                        }
                        transform.UpdateCachedTransform(lock);
                    }
                }
            }},
        {"sun",
            [](Lock<WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
                if (ent.Has<Transform, SignalOutput>(lock)) {
                    auto &transform = ent.Get<Transform>(lock);
                    auto &signalComp = ent.Get<SignalOutput>(lock);

                    auto sunPos = signalComp.GetSignal("position");
                    if (signalComp.GetSignal("fix_position") == 0.0) {
                        sunPos += dtSinceLastFrame * (0.05 + std::abs(sin(sunPos) * 0.1));
                        if (sunPos > M_PI_2) sunPos = -M_PI_2;
                        signalComp.SetSignal("position", sunPos);
                    }

                    transform.SetRotation(glm::quat());
                    transform.Rotate(glm::radians(-90.0), glm::vec3(1, 0, 0));
                    transform.Rotate(sunPos, glm::vec3(0, 1, 0));
                    transform.SetPosition(glm::vec3(sin(sunPos) * 40.0, cos(sunPos) * 40.0, 0));
                }
            }},
        {"light_sensor",
            [](Lock<WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
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
            [](Lock<WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
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
            [](Lock<WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
                if (ent.Has<Script, Transform>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto parentName = scriptComp.GetParam<std::string>("attach_parent");
                    auto parentEntity = scriptComp.GetParam<NamedEntity>("attach_parent_entity");
                    if (parentEntity.Name() != parentName) parentEntity = NamedEntity(parentName);

                    auto &transform = ent.Get<Transform>(lock);
                    auto parent = parentEntity.Get(lock);
                    if (parent.Has<Transform>(lock)) {
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
                    transform.SetParent(parent);
                    transform.UpdateCachedTransform(lock);
                    scriptComp.SetParam<NamedEntity>("attach_parent_entity", parentEntity);
                }
            }},
        {"relative_movement",
            [](Lock<WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
                if (ent.Has<Script, SignalOutput>(lock)) {
                    auto &scriptComp = ent.Get<Script>(lock);
                    auto targetName = scriptComp.GetParam<std::string>("target");
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

                        movement.x = std::clamp(movement.x, -1.0f, 1.0f);
                        movement.z = std::clamp(movement.z, -1.0f, 1.0f);

                        if (target.Has<Transform>(lock)) {
                            auto &parentTransform = target.Get<Transform>(lock);
                            auto parentRotation = parentTransform.GetGlobalRotation(lock);
                            movement = parentRotation * movement;
                            if (std::abs(movement.y) > 0.999) {
                                movement = parentRotation * glm::vec3(0, -movement.y, 0);
                            }
                            movement.y = 0;
                        }

                        auto &outputComp = ent.Get<SignalOutput>(lock);
                        outputComp.SetSignal("move_world_x", movement.x);
                        outputComp.SetSignal("move_world_z", movement.z);
                    }
                }
            }},
        {"model_spawner",
            [](Lock<WriteAll> lock, Tecs::Entity ent, double dtSinceLastFrame) {
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
                                auto &transform = newEntity.Set<Transform>(lock);

                                auto relativeName = scriptComp.GetParam<std::string>("relative_to");
                                if (!relativeName.empty()) {
                                    auto relativeEnt = EntityWith<Name>(lock, relativeName);
                                    if (relativeEnt.Has<Transform>(lock)) {
                                        auto &relativeTransform = relativeEnt.Get<Transform>(lock);
                                        transform.SetRotation(relativeTransform.GetGlobalRotation(lock));
                                        position = relativeTransform.GetGlobalTransform(lock) *
                                                   glm::vec4(position, 1.0f);
                                    }
                                }
                                transform.SetPosition(position);

                                auto parentName = scriptComp.GetParam<std::string>("parent");
                                if (!parentName.empty()) transform.SetParent(EntityWith<Name>(lock, parentName));

                                auto modelName = scriptComp.GetParam<std::string>("model");
                                auto model = sp::GAssets.LoadModel(modelName);
                                newEntity.Set<Renderable>(lock, model);
                                newEntity.Set<Physics>(lock, model, true);
                            }
                        }).detach();
                    }
                }
            }},
    };
} // namespace ecs
