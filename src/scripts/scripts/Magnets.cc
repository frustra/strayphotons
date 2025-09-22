/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/ScriptImpl.hh"
#include "input/BindingNames.hh"

#include <glm/gtx/component_wise.hpp>
#include <unordered_set>

namespace sp::scripts {
    using namespace ecs;

    struct MagneticPlug {
        EntityRef attachedSocketEntity;
        bool disabled = false;
        robin_hood::unordered_flat_set<Entity> grabEntities;
        robin_hood::unordered_flat_set<Entity> socketEntities;

        glm::quat CalcSnapRotation(glm::quat plug, glm::quat socket, float snapAngle) {
            auto plugRelativeSocket = (glm::inverse(socket) * plug) * glm::vec3(0, 0, -1);
            if (std::abs(plugRelativeSocket.y) > 0.999) {
                plugRelativeSocket = (glm::inverse(socket) * plug) * glm::vec3(0, -plugRelativeSocket.y, 0);
            }
            plugRelativeSocket.y = 0;
            plugRelativeSocket = glm::normalize(plugRelativeSocket);
            auto yaw = glm::atan(plugRelativeSocket.x, -plugRelativeSocket.z);
            if (snapAngle > 0.0f) {
                auto rounded = std::round(yaw / snapAngle) * snapAngle;
                return glm::quat(glm::vec3(0, rounded, 0));
            } else {
                return glm::quat(glm::vec3(0, yaw, 0));
            }
        }

        void OnTick(ScriptState &state,
            Lock<ReadSignalsLock, Read<TransformSnapshot>, Write<PhysicsJoints>> lock,
            Entity ent,
            chrono_clock::duration interval) {
            if (!ent.Has<PhysicsJoints, TransformSnapshot>(lock)) return;
            auto &joints = ent.Get<PhysicsJoints>(lock);
            auto &plugTransform = ent.Get<TransformSnapshot>(lock).globalPose;

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name == "/magnet/nearby") {
                    if (event.data.type == EventDataType::Bool) {
                        if (event.data.b) {
                            socketEntities.emplace(event.source);
                        } else {
                            socketEntities.erase(event.source);
                        }
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                    if (disabled) continue;
                    if (event.data.type == EventDataType::Bool) {
                        // Grab(false) = Drop
                        if (!event.data.b) {
                            grabEntities.erase(event.source);

                            if (grabEntities.empty() && !attachedSocketEntity && !socketEntities.empty()) {
                                Entity nearestSocket;
                                float nearestDist = -1;
                                for (auto &entity : socketEntities) {
                                    if (!entity.Has<TransformSnapshot>(lock)) continue;

                                    auto &socketTransform = entity.Get<TransformSnapshot>(lock).globalPose;
                                    float distance = glm::length(
                                        socketTransform.GetPosition() - plugTransform.GetPosition());
                                    if (!nearestSocket.Has<TransformSnapshot>(lock) || distance < nearestDist) {
                                        nearestSocket = entity;
                                        nearestDist = distance;
                                    }
                                }
                                if (nearestSocket.Has<TransformSnapshot>(lock)) {
                                    auto &socketTransform = nearestSocket.Get<TransformSnapshot>(lock).globalPose;

                                    float snapAngle = SignalRef(nearestSocket, "snap_angle").GetSignal(lock);

                                    PhysicsJoint joint;
                                    joint.target = nearestSocket;
                                    joint.type = PhysicsJointType::Fixed;
                                    joint.localOffset.SetRotation(CalcSnapRotation(plugTransform.GetRotation(),
                                        socketTransform.GetRotation(),
                                        glm::radians(snapAngle)));
                                    joints.Add(joint);

                                    attachedSocketEntity = nearestSocket;
                                }
                            }
                        }
                    } else if (event.data.type == EventDataType::Transform) {
                        if (attachedSocketEntity) {
                            Debugf("Detaching: %s from %s",
                                ecs::ToString(lock, ent),
                                attachedSocketEntity.Name().String());

                            sp::erase_if(joints.joints, [&](auto &&joint) {
                                return joint.target == attachedSocketEntity;
                            });

                            attachedSocketEntity = {};
                        }

                        grabEntities.emplace(event.source);
                    } else {
                        Errorf("Unsupported grab event type: %s", event.ToString());
                    }
                }
            }
        }
    };
    StructMetadata MetadataMagneticPlug(typeid(MagneticPlug),
        sizeof(MagneticPlug),
        "MagneticPlug",
        "",
        StructField::New("attach", &MagneticPlug::attachedSocketEntity),
        StructField::New("disabled", &MagneticPlug::disabled));
    LogicScript<MagneticPlug> magneticPlug("magnetic_plug",
        MetadataMagneticPlug,
        true,
        "/magnet/nearby",
        INTERACT_EVENT_INTERACT_GRAB);

    struct MagneticSocket {
        robin_hood::unordered_flat_set<Entity> disabledEntities;

        void OnTick(ScriptState &state, Lock<SendEventsLock> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TriggerArea>(lock)) return;

            EntityRef enableTriggerEntity = ecs::Name("enable_trigger", state.scope);
            auto enableTrigger = enableTriggerEntity.Get(lock);
            if (!enableTrigger.Has<TriggerArea>(lock)) return;

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                auto *data = EventData::TryGet<Entity>(event.data);
                if (!data) continue;

                if (event.name == "/trigger/magnetic/leave") {
                    if (event.source == enableTrigger) {
                        disabledEntities.erase(*data);
                        EventBindings::SendEvent(lock, *data, Event{"/magnet/nearby", ent, false});
                    }
                } else if (event.name == "/trigger/magnetic/enter") {
                    if (event.source == ent && !disabledEntities.contains(*data)) {
                        disabledEntities.emplace(*data);
                        EventBindings::SendEvent(lock, *data, Event{"/magnet/nearby", ent, true});
                    }
                }
            }
        }
    };
    StructMetadata MetadataMagneticSocket(typeid(MagneticSocket), sizeof(MagneticSocket), "MagneticSocket", "");
    LogicScript<MagneticSocket> magneticSocket("magnetic_socket",
        MetadataMagneticSocket,
        true,
        "/trigger/magnetic/enter",
        "/trigger/magnetic/leave");
} // namespace sp::scripts
