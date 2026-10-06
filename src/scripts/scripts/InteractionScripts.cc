/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "console/CVar.hh"
#include "ecs/DynamicLibrary.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptImpl.hh"
#include "ecs/components/Transform.h"
#include "glm/geometric.hpp"
#include "strayphotons/HeapVector.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"
#include "strayphotons/input/BindingNames.hh"

#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    static CVar<float> CVarMaxGrabForce("i.MaxGrabForce", 20.0f, "Maximum force applied to held objects");
    static CVar<float> CVarMaxGrabTorque("i.MaxGrabTorque", 10.0f, "Maximum torque applied to held objects");
    static CVar<float> CVarCarryWeightLimit("i.CarryWeightLimit",
        250.0f,
        "Items heavier than this aren't consider held objects");
    static CVar<bool> CVarFixedJointGrab("i.FixedJointGrab",
        false,
        "Toggle to use a fixed joint instead of force limited joint");

    struct InteractiveObject {
        bool disabled = false;
        bool highlightOnly = false;
        HeapVector<std::pair<EntityRef, EntityRef>> grabEntities;
        HeapVector<EntityRef> pointEntities;
        bool renderOutline = false;
        PhysicsQuery::Handle<PhysicsQuery::Mass> massQuery;

        void OnEvent(ScriptState &state,
            Lock<ReadSignalsLock,
                Read<TransformTree, TransformSnapshot>,
                Write<Renderable, Physics, PhysicsQuery, PhysicsJoints, Signals>> lock,
            Entity ent,
            Event event) {
            if (!ent.Has<TransformSnapshot>(lock)) return;

            bool enableInteraction = !highlightOnly && !disabled;
            if (ent.Has<Physics, PhysicsJoints>(lock)) {
                auto &ph = ent.Get<const Physics>(lock);
                enableInteraction &= ph.type == PhysicsActorType::Dynamic;
            }

            if (event.name == INTERACT_EVENT_INTERACT_POINT) {
                auto *pointTransform = EventData::TryGet<Transform>(event.data);
                if (pointTransform) {
                    if (!sp::contains(pointEntities, event.source)) {
                        pointEntities.emplace_back(event.source);
                    }
                } else if (event.data.type == EventDataType::Bool) {
                    sp::erase(pointEntities, event.source);
                } else {
                    Errorf("Unsupported point event type: %s", event.ToString());
                }
            } else if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                if (event.data.type == EventDataType::Bool) {
                    // Grab(false) = Drop
                    EntityRef secondary;
                    for (auto &[a, b] : grabEntities) {
                        if (a == event.source) {
                            secondary = b;
                            break;
                        }
                    }
                    sp::erase_if(grabEntities, [&](auto &arg) {
                        return arg.first == event.source;
                    });
                    if (ent.Has<PhysicsJoints>(lock)) {
                        auto &joints = ent.Get<PhysicsJoints>(lock);
                        sp::erase_if(joints.joints, [&](auto &&joint) {
                            return joint.target == event.source || (secondary && joint.target == secondary);
                        });
                    }
                } else if (event.data.type == EventDataType::Transform) {
                    if (!enableInteraction) return;

                    auto &parentTransform = event.data.transform;
                    auto &transform = ent.Get<TransformSnapshot>(lock).globalPose;
                    auto invParentRotate = glm::inverse(parentTransform.GetRotation());

                    EntityRef secondary;
                    if (ent.Has<PhysicsJoints>(lock)) {
                        auto &joints = ent.Get<PhysicsJoints>(lock);
                        if (event.source.Has<PhysicsJoints>(lock)) {
                            auto &targetJoints = event.source.Get<const PhysicsJoints>(lock);
                            for (auto &joint : targetJoints.joints) {
                                if (joint.type != PhysicsJointType::Force) continue;
                                auto target = joint.target.Get(lock);
                                if (target.Has<TransformSnapshot>(lock) && !target.Has<Physics>(lock)) {
                                    secondary = target;

                                    PhysicsJoint newJoint = joint;
                                    newJoint.remoteOffset.Translate(
                                        invParentRotate * (transform.GetPosition() - parentTransform.GetPosition()));
                                    newJoint.remoteOffset.Rotate(invParentRotate * transform.GetRotation());
                                    // Logf("Adding secondary joint: %s / %s",
                                    //     newJoint.type,
                                    //     newJoint.target.Name().String());
                                    joints.Add(newJoint);

                                    break;
                                }
                            }
                        }

                        PhysicsJoint joint;
                        joint.target = event.source;
                        if (secondary) {
                            joint.type = PhysicsJointType::Fixed;
                        } else {
                            joint.type = PhysicsJointType::Force;
                            // TODO: Read this property from player
                            joint.limit = glm::vec2(CVarMaxGrabForce.Get(), CVarMaxGrabTorque.Get());
                        }
                        joint.remoteOffset.SetPosition(
                            invParentRotate * (transform.GetPosition() - parentTransform.GetPosition()));
                        joint.remoteOffset.SetRotation(invParentRotate * transform.GetRotation());
                        joints.Add(joint);
                    }

                    grabEntities.emplace_back(event.source, secondary);
                } else {
                    Errorf("Unsupported grab event type: %s", event.ToString());
                }
            } else if (event.name == INTERACT_EVENT_INTERACT_PUSH) {
                if (!ent.Has<Physics>(lock) || !enableInteraction) return;

                if (event.data.type == EventDataType::Vec3) {
                    auto &force = event.data.vec3;

                    auto &physics = ent.Get<Physics>(lock);
                    physics.constantForce = force;
                }
                // } else if (event.name == INTERACT_EVENT_INTERACT_ROTATE) {
                //     if (!ent.Has<Physics, PhysicsJoints>(lock) || !enableInteraction) return;

                //     if (event.data.type == EventDataType::Vec2) {
                //         if (!event.source.Has<TransformSnapshot>(lock)) return;

                //         auto &input = event.data.vec2;
                //         auto &transform = event.source.Get<TransformSnapshot>(lock).globalPose;

                //         auto upAxis = glm::inverse(transform.GetRotation()) * glm::vec3(0, 1, 0);
                //         auto deltaRotate = glm::angleAxis(input.y, glm::vec3(1, 0, 0)) * glm::angleAxis(input.x,
                //         upAxis);

                //         auto &joints = ent.Get<PhysicsJoints>(lock);
                //         for (auto &joint : joints.joints) {
                //             if (joint.target == event.source) {
                //                 // Move the objects origin so it rotates around its center of mass
                //                 auto center = joint.remoteOffset.GetRotation() * centerOfMass;
                //                 joint.remoteOffset.Translate(center - (deltaRotate * center));
                //                 joint.remoteOffset.SetRotation(deltaRotate * joint.remoteOffset.GetRotation());
                //             }
                //         }
                //     }
            }
        }

        void OnTick(ScriptState &state,
            Lock<ReadSignalsLock,
                Read<TransformTree, TransformSnapshot>,
                Write<Renderable, Physics, PhysicsQuery, PhysicsJoints, Signals>> lock,
            Entity ent,
            chrono_clock::duration interval) {
            if (!ent.Has<TransformSnapshot>(lock)) return;

            bool enableInteraction = !highlightOnly && !disabled;
            if (ent.Has<Physics, PhysicsJoints>(lock)) {
                auto &ph = ent.Get<const Physics>(lock);
                enableInteraction &= ph.type == PhysicsActorType::Dynamic;
            }

            glm::vec3 centerOfMass = glm::vec3(0);
            float actualMass = 0.0f;
            if (enableInteraction && ent.Has<PhysicsQuery>(lock)) {
                auto &query = ent.Get<PhysicsQuery>(lock);
                if (!massQuery) {
                    massQuery = query.NewQuery(PhysicsQuery::Mass(ent));
                } else {
                    auto &result = query.Lookup(massQuery).result;
                    if (result) {
                        centerOfMass = result->centerOfMass;
                        actualMass = result->weight;
                    }
                }
            }

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {}

            if (ent.Has<Physics>(lock)) {
                auto &ph = ent.Get<const Physics>(lock);
                if (grabEntities.empty() && ph.group == PhysicsGroup::HeldObject) {
                    ent.Get<Physics>(lock).group = PhysicsGroup::World;
                } else if (!grabEntities.empty() && ph.group == PhysicsGroup::World) {
                    if (actualMass > 0.0f && actualMass <= CVarCarryWeightLimit.Get()) {
                        ent.Get<Physics>(lock).group = PhysicsGroup::HeldObject;
                    }
                }
            }

            bool newRenderOutline = (enableInteraction || highlightOnly) &&
                                    (!grabEntities.empty() || !pointEntities.empty());
            if (renderOutline != newRenderOutline) {
                for (auto &e : lock.EntitiesWith<Renderable>()) {
                    if (!e.Has<TransformTree, Renderable>(lock)) continue;

                    auto child = e;
                    while (child.Has<TransformTree>(lock)) {
                        if (child == ent) {
                            auto &visibility = e.Get<Renderable>(lock).visibility;
                            if (newRenderOutline) {
                                visibility |= VisibilityMask::OutlineSelection;
                            } else {
                                visibility &= ~VisibilityMask::OutlineSelection;
                            }
                            break;
                        }
                        child = child.Get<TransformTree>(lock).parent.Get(lock);
                    }
                }
                renderOutline = newRenderOutline;
            }

            SignalRef(ent, "interact_holds").SetValue(lock, !disabled ? (double)grabEntities.size() : 0.0f);
            SignalRef(ent, "interact_points").SetValue(lock, !disabled ? (double)pointEntities.size() : 0.0f);
        }
    };
    StructMetadata MetadataInteractiveObject(typeid(InteractiveObject),
        sizeof(InteractiveObject),
        "InteractiveObject",
        "",
        StructField::New("disabled", &InteractiveObject::disabled),
        StructField::New("highlight_only", &InteractiveObject::highlightOnly),
        StructField::New("_grab_entities", &InteractiveObject::grabEntities),
        StructField::New("_point_entities", &InteractiveObject::pointEntities),
        StructField::New("_render_outline", &InteractiveObject::renderOutline));
    EventScript<InteractiveObject> interactiveObject("interactive_object",
        MetadataInteractiveObject,
        INTERACT_EVENT_INTERACT_POINT,
        INTERACT_EVENT_INTERACT_GRAB,
        INTERACT_EVENT_INTERACT_PUSH,
        INTERACT_EVENT_INTERACT_ROTATE);

    struct InteractHandler {
        float grabDistance = 2.0f;
        EntityRef noclipEntity;
        EntityRef grabEntity, pointEntity, pressEntity;
        PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;

        void UpdateGrabTarget(Lock<Write<PhysicsJoints>> lock, Entity newGrabEntity) {
            auto noclipEnt = noclipEntity.Get(lock);
            if (!noclipEnt.Has<PhysicsJoints>(lock)) return;
            auto &joints = noclipEnt.Get<PhysicsJoints>(lock);
            PhysicsJoint joint;
            joint.type = PhysicsJointType::NoClip;
            joint.target = grabEntity;
            if (newGrabEntity == grabEntity) {
                if (grabEntity) joints.Add(joint);
            } else {
                auto it = std::find(joints.joints.begin(), joints.joints.end(), joint);
                if (it != joints.joints.end()) {
                    it->type = PhysicsJointType::TemporaryNoClip;
                }
                if (newGrabEntity) {
                    joint.target = newGrabEntity;
                    joints.Add(joint);
                }
                grabEntity = newGrabEntity;
            }
        }

        void OnTick(ScriptState &state,
            Lock<SendEventsLock, Read<TransformSnapshot>, Write<PhysicsQuery, PhysicsJoints>> lock,
            Entity ent,
            chrono_clock::duration interval) {
            if (ent.Has<TransformSnapshot, PhysicsQuery>(lock)) {
                auto &query = ent.Get<PhysicsQuery>(lock);
                auto &transform = ent.Get<TransformSnapshot>(lock).globalPose;

                PhysicsQuery::Raycast::Result raycastResult = {};
                if (!raycastQuery) {
                    raycastQuery = query.NewQuery(PhysicsQuery::Raycast(grabDistance,
                        PhysicsGroupMask(PHYSICS_GROUP_WORLD | PHYSICS_GROUP_INTERACTIVE |
                                         PHYSICS_GROUP_USER_INTERFACE | PHYSICS_GROUP_HELD_OBJECT)));
                } else {
                    auto &result = query.Lookup(raycastQuery).result;
                    if (result) raycastResult = result.value();
                }

                bool rotating = SignalRef(ent, "interact_rotate").GetSignal(lock) >= 0.5;

                Event event;
                while (EventInput::Poll(lock, state.eventQueue, event)) {
                    if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                        auto justDropped = grabEntity;
                        if (grabEntity) {
                            // Drop the currently held entity
                            size_t count = EventBindings::SendEvent(lock,
                                grabEntity,
                                Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                            UpdateGrabTarget(lock, {});
                            if (count == 0) {
                                justDropped = Entity(); // Held entity no longer exists
                            }
                        }
                        if (event.data.type == EventDataType::Bool) {
                            auto &grabEvent = event.data.b;
                            if (grabEvent && raycastResult.target && !justDropped) {
                                // Grab the entity being looked at
                                if (EventBindings::SendEvent(lock,
                                        raycastResult.target,
                                        Event{INTERACT_EVENT_INTERACT_GRAB, ent, transform}) > 0) {
                                    UpdateGrabTarget(lock, raycastResult.target);
                                }
                            }
                        } else if (event.data.type == EventDataType::Entity) {
                            auto &targetEnt = event.data.ent;
                            if (targetEnt) {
                                // Grab the entity requested by the event
                                if (EventBindings::SendEvent(lock,
                                        targetEnt,
                                        Event{INTERACT_EVENT_INTERACT_GRAB, ent, transform}) > 0) {
                                    UpdateGrabTarget(lock, targetEnt);
                                }
                            }
                        } else {
                            Errorf("Unsupported grab event type: %s", event.ToString());
                        }
                    } else if (event.name == INTERACT_EVENT_INTERACT_PRESS) {
                        if (event.data.type == EventDataType::Bool) {
                            if (pressEntity) {
                                // Unpress the currently pressed entity
                                EventBindings::SendEvent(lock,
                                    pressEntity,
                                    Event{INTERACT_EVENT_INTERACT_PRESS, ent, false});
                                pressEntity = {};
                            }
                            if (event.data.b && raycastResult.target) {
                                // Press the entity being looked at
                                EventBindings::SendEvent(lock,
                                    raycastResult.target,
                                    Event{INTERACT_EVENT_INTERACT_PRESS, ent, true});
                                pressEntity = raycastResult.target;
                            }
                        }
                    } else if (event.name == INTERACT_EVENT_INTERACT_ROTATE) {
                        if (rotating && grabEntity) {
                            EventBindings::SendEvent(lock,
                                grabEntity,
                                Event{INTERACT_EVENT_INTERACT_ROTATE, ent, event.data});
                        }
                    }
                }

                if (pointEntity && raycastResult.target != pointEntity) {
                    EventBindings::SendEvent(lock, pointEntity, Event{INTERACT_EVENT_INTERACT_POINT, ent, false});
                }
                if (raycastResult.target) {
                    Transform pointTransfrom = transform;
                    pointTransfrom.SetPosition(raycastResult.position);
                    EventBindings::SendEvent(lock,
                        raycastResult.target,
                        Event{INTERACT_EVENT_INTERACT_POINT, ent, pointTransfrom});
                }
                pointEntity = raycastResult.target;
            }
        }
    };
    StructMetadata MetadataInteractHandler(typeid(InteractHandler),
        sizeof(InteractHandler),
        "InteractHandler",
        "",
        StructField::New("grab_distance", &InteractHandler::grabDistance),
        StructField::New("noclip_entity", &InteractHandler::noclipEntity),
        StructField::New("_grab_entity", &InteractHandler::grabEntity),
        StructField::New("_point_entity", &InteractHandler::pointEntity),
        StructField::New("_press_entity", &InteractHandler::pressEntity));
    LogicScript<InteractHandler> interactHandler("interact_handler",
        MetadataInteractHandler,
        false,
        INTERACT_EVENT_INTERACT_GRAB,
        INTERACT_EVENT_INTERACT_PRESS,
        INTERACT_EVENT_INTERACT_ROTATE);

    struct MultiInteractHandler {
        EntityRef jointEntity;
        HeapVector<EntityRef> grabEntities;

        void OnTick(ScriptState &state,
            DynamicLock<SendEventsLock, Read<TransformSnapshot, LightCast>, Write<PhysicsJoints>> lock,
            Entity ent,
            chrono_clock::duration interval) {
            if (ent.Has<TransformSnapshot, LightCast>(lock)) {
                const LightCast &cast = ent.Get<LightCast>(lock);
                const Transform &transform = ent.Get<TransformSnapshot>(lock).globalPose;

                // bool grabbing = !grabEntities.empty();
                // Event event;
                // while (EventInput::Poll(lock, state.eventQueue, event)) {
                //     if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                //         if (event.data.type == EventDataType::Bool) {
                //             auto &grabEvent = event.data.b;
                //             grabbing = grabEvent && !grabbing;
                //         } else {
                //             Errorf("Unsupported grab event type: %s", event.ToString());
                //         }
                //     }
                // }
                bool grabbing = SignalRef(ent, "interact_grab").GetSignal(lock) > 0.5;
                float pullSpeed = SignalRef(ent, "interact_pull").GetSignal(lock);
                if (pullSpeed != 0.0f) grabbing = true;

                if (!grabbing && !grabEntities.empty()) {
                    // Drop the currently held entities
                    for (const EntityRef &ref : grabEntities) {
                        EventBindings::SendEvent(lock, ref, Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                    }
                    grabEntities.clear();
                } else if (grabbing) {
                    if (pullSpeed != 0.0f) {
                        // Temporarily drop any entities from last frame
                        for (const EntityRef &ref : grabEntities) {
                            EventBindings::SendEvent(lock, ref, Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                        }
                        grabEntities.clear();
                    } else {
                        // Drop any entities that are no longer visible
                        sp::erase_if(grabEntities, [&](auto &ref) {
                            if (cast.visibleEntities.count(ref.Get(lock)) == 0) {
                                EventBindings::SendEvent(lock, ref, Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                                return true;
                            }
                            return false;
                        });
                    }
                    // Grab any new entities being looked at
                    for (auto &pair : cast.visibleEntities) {
                        Entity target = pair.first;
                        if (pullSpeed == 0.0f && sp::contains(grabEntities, target)) continue;
                        Transform offset = transform;
                        if (pullSpeed != 0.0f) {
                            offset.Translate(transform.GetForward() * pullSpeed);
                        }
                        if (EventBindings::SendEvent(lock, target, Event{INTERACT_EVENT_INTERACT_GRAB, ent, offset}) >
                            0) {
                            grabEntities.emplace_back(pair.first);
                        }
                    }
                }
            }
        }
    };
    StructMetadata MetadataMultiInteractHandler(typeid(MultiInteractHandler),
        sizeof(MultiInteractHandler),
        "MultiInteractHandler",
        "",
        StructField::New("joint_entity", &MultiInteractHandler::jointEntity),
        StructField::New("_grab_entities", &MultiInteractHandler::grabEntities));
    LogicScript<MultiInteractHandler> multiInteractHandler("multi_interact_handler",
        MetadataMultiInteractHandler,
        false,
        INTERACT_EVENT_INTERACT_GRAB);
} // namespace sp::scripts
