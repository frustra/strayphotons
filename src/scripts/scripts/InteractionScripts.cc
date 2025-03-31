/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "common/Logging.hh"
#include "console/CVar.hh"
#include "ecs/EcsImpl.hh"
#include "input/BindingNames.hh"

#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    static CVar<float> CVarMaxGrabForce("i.MaxGrabForce", 20.0f, "Maximum force applied to held objects");
    static CVar<float> CVarMaxGrabTorque("i.MaxGrabTorque", 10.0f, "Maximum torque applied to held objects");
    static CVar<bool> CVarFixedJointGrab("i.FixedJointGrab",
        false,
        "Toggle to use a fixed joint instead of force limited joint");

    struct InteractiveObject {
        bool disabled = false;
        bool highlightOnly = false;
        std::vector<std::pair<EntityRef, EntityRef>> grabEntities;
        std::vector<EntityRef> pointEntities;
        bool renderOutline = false;
        PhysicsQuery::Handle<PhysicsQuery::Mass> massQuery;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformSnapshot>(lock)) return;

            bool enableInteraction = !highlightOnly && !disabled;
            if (ent.Has<Physics, PhysicsJoints>(lock)) {
                auto &ph = ent.Get<Physics>(lock);
                enableInteraction &= ph.type == PhysicsActorType::Dynamic;
            }

            glm::vec3 centerOfMass = glm::vec3(0);
            if (enableInteraction && ent.Has<PhysicsQuery>(lock)) {
                auto &query = ent.Get<PhysicsQuery>(lock);
                if (!massQuery) {
                    massQuery = query.NewQuery(PhysicsQuery::Mass(ent));
                } else {
                    auto &result = query.Lookup(massQuery).result;
                    if (result) centerOfMass = result->centerOfMass;
                }
            }

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name == INTERACT_EVENT_INTERACT_POINT) {
                    auto pointTransform = std::get_if<Transform>(&event.data);
                    if (pointTransform) {
                        if (!sp::contains(pointEntities, event.source)) {
                            pointEntities.emplace_back(event.source);
                        }
                    } else if (std::holds_alternative<bool>(event.data)) {
                        sp::erase(pointEntities, event.source);
                    } else {
                        Errorf("Unsupported point event type: %s", event.ToString());
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                    if (std::holds_alternative<bool>(event.data)) {
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
                    } else if (std::holds_alternative<Transform>(event.data)) {
                        if (!enableInteraction) continue;

                        auto &parentTransform = std::get<Transform>(event.data);
                        auto &transform = ent.Get<TransformSnapshot>(lock).globalPose;
                        auto invParentRotate = glm::inverse(parentTransform.GetRotation());

                        EntityRef secondary;
                        if (ent.Has<PhysicsJoints>(lock)) {
                            auto &joints = ent.Get<PhysicsJoints>(lock);
                            if (event.source.Has<PhysicsJoints>(lock)) {
                                auto &targetJoints = event.source.Get<PhysicsJoints>(lock);
                                for (auto &joint : targetJoints.joints) {
                                    if (joint.type != PhysicsJointType::Force) continue;
                                    auto target = joint.target.Get(lock);
                                    if (target.Has<TransformSnapshot>(lock) && !target.Has<Physics>(lock)) {
                                        secondary = target;

                                        PhysicsJoint newJoint = joint;
                                        newJoint.remoteOffset.Translate(
                                            invParentRotate *
                                            (transform.GetPosition() - parentTransform.GetPosition()));
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
                } else if (event.name == INTERACT_EVENT_INTERACT_ROTATE) {
                    if (!ent.Has<Physics, PhysicsJoints>(lock) || !enableInteraction) continue;

                    if (std::holds_alternative<glm::vec2>(event.data)) {
                        if (!event.source.Has<TransformSnapshot>(lock)) continue;

                        auto &input = std::get<glm::vec2>(event.data);
                        auto &transform = event.source.Get<const TransformSnapshot>(lock).globalPose;

                        auto upAxis = glm::inverse(transform.GetRotation()) * glm::vec3(0, 1, 0);
                        auto deltaRotate = glm::angleAxis(input.y, glm::vec3(1, 0, 0)) *
                                           glm::angleAxis(input.x, upAxis);

                        auto &joints = ent.Get<PhysicsJoints>(lock);
                        for (auto &joint : joints.joints) {
                            if (joint.target == event.source) {
                                // Move the objects origin so it rotates around its center of mass
                                auto center = joint.remoteOffset.GetRotation() * centerOfMass;
                                joint.remoteOffset.Translate(center - (deltaRotate * center));
                                joint.remoteOffset.SetRotation(deltaRotate * joint.remoteOffset.GetRotation());
                            }
                        }
                    }
                }
            }

            if (ent.Has<Physics>(lock)) {
                auto &ph = ent.Get<Physics>(lock);
                if (grabEntities.empty() && ph.group == PhysicsGroup::HeldObject) {
                    ph.group = PhysicsGroup::World;
                } else if (!grabEntities.empty() && ph.group == PhysicsGroup::World) {
                    ph.group = PhysicsGroup::HeldObject;
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
        "InteractiveObject",
        "",
        StructField::New("disabled", &InteractiveObject::disabled),
        StructField::New("highlight_only", &InteractiveObject::highlightOnly),
        StructField::New("_grab_entities", &InteractiveObject::grabEntities),
        StructField::New("_point_entities", &InteractiveObject::pointEntities),
        StructField::New("_render_outline", &InteractiveObject::renderOutline));
    InternalScript<InteractiveObject> interactiveObject("interactive_object",
        MetadataInteractiveObject,
        true,
        INTERACT_EVENT_INTERACT_POINT,
        INTERACT_EVENT_INTERACT_GRAB,
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

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (ent.Has<TransformSnapshot, PhysicsQuery>(lock)) {
                auto &query = ent.Get<PhysicsQuery>(lock);
                auto &transform = ent.Get<TransformSnapshot>(lock).globalPose;

                PhysicsQuery::Raycast::Result raycastResult = {};
                if (!raycastQuery) {
                    raycastQuery = query.NewQuery(PhysicsQuery::Raycast(grabDistance,
                        PhysicsGroupMask(
                            PHYSICS_GROUP_WORLD | PHYSICS_GROUP_INTERACTIVE | PHYSICS_GROUP_USER_INTERFACE)));
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
                        if (std::holds_alternative<bool>(event.data)) {
                            auto &grabEvent = std::get<bool>(event.data);
                            if (grabEvent && raycastResult.target && !justDropped) {
                                // Grab the entity being looked at
                                if (EventBindings::SendEvent(lock,
                                        raycastResult.target,
                                        Event{INTERACT_EVENT_INTERACT_GRAB, ent, transform}) > 0) {
                                    UpdateGrabTarget(lock, raycastResult.target);
                                }
                            }
                        } else if (std::holds_alternative<Entity>(event.data)) {
                            auto &targetEnt = std::get<Entity>(event.data);
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
                        if (std::holds_alternative<bool>(event.data)) {
                            if (pressEntity) {
                                // Unpress the currently pressed entity
                                EventBindings::SendEvent(lock,
                                    pressEntity,
                                    Event{INTERACT_EVENT_INTERACT_PRESS, ent, false});
                                pressEntity = {};
                            }
                            if (std::get<bool>(event.data) && raycastResult.target) {
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
        "InteractHandler",
        "",
        StructField::New("grab_distance", &InteractHandler::grabDistance),
        StructField::New("noclip_entity", &InteractHandler::noclipEntity),
        StructField::New("_grab_entity", &InteractHandler::grabEntity),
        StructField::New("_point_entity", &InteractHandler::pointEntity),
        StructField::New("_press_entity", &InteractHandler::pressEntity));
    InternalScript<InteractHandler> interactHandler("interact_handler",
        MetadataInteractHandler,
        false,
        INTERACT_EVENT_INTERACT_GRAB,
        INTERACT_EVENT_INTERACT_PRESS,
        INTERACT_EVENT_INTERACT_ROTATE);
} // namespace sp::scripts
