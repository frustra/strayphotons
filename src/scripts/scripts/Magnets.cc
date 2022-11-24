#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

#include <glm/gtx/component_wise.hpp>
#include <unordered_set>

namespace sp::scripts {
    using namespace ecs;

    glm::quat calcSnapRotation(glm::quat plug, glm::quat socket, float snapAngle = glm::radians(45.0f)) {
        auto plugRelativeSocket = (glm::inverse(socket) * plug) * glm::vec3(0, 0, -1);
        if (std::abs(plugRelativeSocket.y) > 0.999) {
            plugRelativeSocket = (glm::inverse(socket) * plug) * glm::vec3(0, -plugRelativeSocket.y, 0);
        }
        plugRelativeSocket.y = 0;
        plugRelativeSocket = glm::normalize(plugRelativeSocket);
        auto yaw = glm::atan(plugRelativeSocket.x, -plugRelativeSocket.z);
        auto rounded = std::round(yaw / snapAngle) * snapAngle;
        return glm::quat(glm::vec3(0, rounded, 0));
    }

    std::array magnetScripts = {
        InternalScript(
            "magnetic_plug",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (!ent.Has<PhysicsJoints, TransformSnapshot>(lock)) return;
                auto &joints = ent.Get<PhysicsJoints>(lock);
                auto &plugTransform = ent.Get<const TransformSnapshot>(lock);

                struct ScriptData {
                    Entity attachedEntity;
                    robin_hood::unordered_flat_set<Entity> grabEntities;
                    robin_hood::unordered_flat_set<Entity> socketEntities;
                };
                ScriptData scriptData = {};
                if (state.userData.has_value()) scriptData = std::any_cast<ScriptData>(state.userData);

                Event event;
                while (EventInput::Poll(lock, state.eventQueue, event)) {
                    if (event.name == "/magnet/nearby") {
                        if (std::holds_alternative<bool>(event.data)) {
                            if (std::get<bool>(event.data)) {
                                scriptData.socketEntities.emplace(event.source);
                            } else {
                                scriptData.socketEntities.erase(event.source);
                            }
                        }
                    } else if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                        if (std::holds_alternative<bool>(event.data)) {
                            // Grab(false) = Drop
                            if (!std::get<bool>(event.data)) {
                                scriptData.grabEntities.erase(event.source);

                                if (scriptData.grabEntities.empty() && !scriptData.attachedEntity &&
                                    !scriptData.socketEntities.empty()) {

                                    Entity nearestSocket;
                                    float nearestDist = -1;
                                    for (auto &entity : scriptData.socketEntities) {
                                        if (!entity.Has<TransformSnapshot>(lock)) continue;

                                        auto &socketTransform = entity.Get<const TransformSnapshot>(lock);
                                        float distance = glm::length(
                                            socketTransform.GetPosition() - plugTransform.GetPosition());
                                        if (!nearestSocket.Has<TransformSnapshot>(lock) || distance < nearestDist) {
                                            nearestSocket = entity;
                                            nearestDist = distance;
                                        }
                                    }
                                    if (nearestSocket.Has<TransformSnapshot>(lock)) {
                                        auto &socketTransform = nearestSocket.Get<const TransformSnapshot>(lock);

                                        PhysicsJoint joint;
                                        joint.target = nearestSocket;
                                        joint.type = PhysicsJointType::Fixed;
                                        joint.localOrient = calcSnapRotation(plugTransform.GetRotation(),
                                            socketTransform.GetRotation());
                                        joints.Add(joint);

                                        scriptData.attachedEntity = nearestSocket;
                                    }
                                }
                            }
                        } else if (std::holds_alternative<Transform>(event.data)) {
                            if (scriptData.attachedEntity) {
                                Debugf("Detaching: %s from %s",
                                    ecs::ToString(lock, ent),
                                    ecs::ToString(lock, scriptData.attachedEntity));

                                sp::erase_if(joints.joints, [&](auto &&joint) {
                                    return joint.target == scriptData.attachedEntity;
                                });

                                scriptData.attachedEntity = {};
                            }

                            scriptData.grabEntities.emplace(event.source);
                        } else {
                            Errorf("Unsupported grab event type: %s", event.toString());
                        }
                    }
                }

                state.userData = scriptData;
            },
            "/magnet/nearby",
            INTERACT_EVENT_INTERACT_GRAB),
        InternalScript(
            "magnetic_socket",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (!ent.Has<TriggerArea>(lock)) return;

                EntityRef enableTriggerEntity = ecs::Name("enable_trigger", state.scope.prefix);
                auto enableTrigger = enableTriggerEntity.Get(lock);
                if (!enableTrigger.Has<TriggerArea>(lock)) return;

                struct ScriptData {
                    robin_hood::unordered_flat_set<Entity> disabledEntities;
                };
                ScriptData scriptData = {};
                if (state.userData.has_value()) scriptData = std::any_cast<ScriptData>(state.userData);

                Event event;
                while (EventInput::Poll(lock, state.eventQueue, event)) {
                    auto data = std::get_if<Entity>(&event.data);
                    if (!data) continue;

                    if (event.name == "/trigger/magnetic/leave") {
                        if (event.source == enableTrigger) {
                            scriptData.disabledEntities.erase(*data);
                            EventBindings::SendEvent(lock, *data, Event{"/magnet/nearby", ent, false});
                        }
                    } else if (event.name == "/trigger/magnetic/enter") {
                        if (event.source == ent && !scriptData.disabledEntities.contains(*data)) {
                            scriptData.disabledEntities.emplace(*data);
                            EventBindings::SendEvent(lock, *data, Event{"/magnet/nearby", ent, true});
                        }
                    }
                }

                state.userData = scriptData;
            },
            "/trigger/magnetic/enter",
            "/trigger/magnetic/leave"),
    };
} // namespace sp::scripts
