#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

#include <glm/gtx/component_wise.hpp>
#include <unordered_set>

namespace sp::scripts {
    using namespace ecs;

    struct MagneticPlug {
        EntityRef attachedSocketEntity;
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

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<PhysicsJoints, TransformSnapshot>(lock)) return;
            auto &joints = ent.Get<PhysicsJoints>(lock);
            auto &plugTransform = ent.Get<const TransformSnapshot>(lock);

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name == "/magnet/nearby") {
                    if (std::holds_alternative<bool>(event.data)) {
                        if (std::get<bool>(event.data)) {
                            socketEntities.emplace(event.source);
                        } else {
                            socketEntities.erase(event.source);
                        }
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                    if (std::holds_alternative<bool>(event.data)) {
                        // Grab(false) = Drop
                        if (!std::get<bool>(event.data)) {
                            grabEntities.erase(event.source);

                            if (grabEntities.empty() && !attachedSocketEntity && !socketEntities.empty()) {
                                Entity nearestSocket;
                                float nearestDist = -1;
                                for (auto &entity : socketEntities) {
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

                                    float snapAngle = SignalBindings::GetSignal(lock, nearestSocket, "snap_angle");

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
                    } else if (std::holds_alternative<Transform>(event.data)) {
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
                        Errorf("Unsupported grab event type: %s", event.toString());
                    }
                }
            }
        }
    };
    StructMetadata MetadataMagneticPlug(typeid(MagneticPlug),
        StructField::New("attach", &MagneticPlug::attachedSocketEntity));
    InternalScript<MagneticPlug> magneticPlug("magnetic_plug",
        MetadataMagneticPlug,
        true,
        "/magnet/nearby",
        INTERACT_EVENT_INTERACT_GRAB);

    struct MagneticSocket {
        robin_hood::unordered_flat_set<Entity> disabledEntities;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TriggerArea>(lock)) return;

            EntityRef enableTriggerEntity = ecs::Name("enable_trigger", state.scope);
            auto enableTrigger = enableTriggerEntity.Get(lock);
            if (!enableTrigger.Has<TriggerArea>(lock)) return;

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                auto data = std::get_if<Entity>(&event.data);
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
    StructMetadata MetadataMagneticSocket(typeid(MagneticSocket));
    InternalScript<MagneticSocket> magneticSocket("magnetic_socket",
        MetadataMagneticSocket,
        true,
        "/trigger/magnetic/enter",
        "/trigger/magnetic/leave");
} // namespace sp::scripts
