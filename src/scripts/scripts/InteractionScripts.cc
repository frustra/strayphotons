#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

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
        std::vector<std::pair<Entity, Entity>> grabEntities;
        std::vector<Entity> pointEntities;
        bool renderOutline = false;
        PhysicsQuery::Handle<PhysicsQuery::Mass> massQuery;

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (!entLock.Has<TransformSnapshot, Physics, PhysicsJoints>()) return;

            auto &ph = entLock.Get<Physics>();
            auto &joints = entLock.Get<PhysicsJoints>();
            bool enableInteraction = ph.type == PhysicsActorType::Dynamic && !disabled;

            glm::vec3 centerOfMass;
            if (enableInteraction && entLock.Has<PhysicsQuery>()) {
                auto &query = entLock.Get<PhysicsQuery>();
                if (massQuery) {
                    auto &result = query.Lookup(massQuery).result;
                    if (result) centerOfMass = result->centerOfMass;
                } else {
                    massQuery = query.NewQuery(PhysicsQuery::Mass(entLock.entity));
                }
            }

            Event event;
            while (EventInput::Poll(entLock, state.eventQueue, event)) {
                if (event.name == INTERACT_EVENT_INTERACT_POINT) {
                    auto pointTransform = std::get_if<Transform>(&event.data);
                    if (pointTransform) {
                        pointEntities.emplace_back(event.source);
                    } else if (std::holds_alternative<bool>(event.data)) {
                        sp::erase(pointEntities, event.source);
                    } else {
                        Errorf("Unsupported point event type: %s", event.toString());
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                    if (std::holds_alternative<bool>(event.data)) {
                        // Grab(false) = Drop
                        Entity secondary;
                        for (auto &[a, b] : grabEntities) {
                            if (a == event.source) {
                                secondary = b;
                                break;
                            }
                        }
                        sp::erase_if(joints.joints, [&](auto &&joint) {
                            return joint.target == event.source || (secondary && joint.target == secondary);
                        });
                        sp::erase_if(grabEntities, [&](auto &arg) {
                            return arg.first == event.source;
                        });
                    } else if (std::holds_alternative<Transform>(event.data)) {
                        if (!enableInteraction) continue;

                        auto &parentTransform = std::get<Transform>(event.data);
                        auto &transform = entLock.Get<TransformSnapshot>();
                        auto invParentRotate = glm::inverse(parentTransform.GetRotation());

                        Entity secondary;
                        if (event.source.Has<PhysicsJoints>(entLock)) {
                            auto &targetJoints = event.source.Get<PhysicsJoints>(entLock);
                            for (auto &joint : targetJoints.joints) {
                                if (joint.type != PhysicsJointType::Force) continue;
                                auto target = joint.target.Get(entLock);
                                if (target.Has<TransformSnapshot>(entLock) && !target.Has<Physics>(entLock)) {
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
                        grabEntities.emplace_back(event.source, secondary);

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
                    } else {
                        Errorf("Unsupported grab event type: %s", event.toString());
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_ROTATE) {
                    if (std::holds_alternative<glm::vec2>(event.data)) {
                        if (!enableInteraction) continue;
                        if (!event.source.Has<TransformSnapshot>(entLock)) continue;

                        auto &input = std::get<glm::vec2>(event.data);
                        auto &transform = event.source.Get<const TransformSnapshot>(entLock);

                        auto upAxis = glm::inverse(transform.GetRotation()) * glm::vec3(0, 1, 0);
                        auto deltaRotate = glm::angleAxis(input.y, glm::vec3(1, 0, 0)) *
                                           glm::angleAxis(input.x, upAxis);

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

            if (grabEntities.empty() && ph.group == PhysicsGroup::HeldObject) {
                ph.group = PhysicsGroup::World;
            } else if (!grabEntities.empty() && ph.group == PhysicsGroup::World) {
                ph.group = PhysicsGroup::HeldObject;
            }

            bool newRenderOutline = !disabled && (!grabEntities.empty() || !pointEntities.empty());
            if (renderOutline != newRenderOutline) {
                for (auto &e : entLock.EntitiesWith<Renderable>()) {
                    if (!e.Has<TransformTree, Renderable>(entLock)) continue;

                    auto child = e;
                    while (child.Has<TransformTree>(entLock)) {
                        if (child == entLock.entity) {
                            if (e != entLock.entity) break;
                            // TODO: Find another way to set outline visibility of children
                            // auto &visibility = e.Get<Renderable>(entLock).visibility;
                            auto &visibility = entLock.Get<Renderable>().visibility;
                            if (newRenderOutline) {
                                visibility |= VisibilityMask::OutlineSelection;
                            } else {
                                visibility &= ~VisibilityMask::OutlineSelection;
                            }
                            break;
                        }
                        child = child.Get<TransformTree>(entLock).parent.Get(entLock);
                    }
                }
                renderOutline = newRenderOutline;
            }
        }
    };
    StructMetadata MetadataInteractiveObject(typeid(InteractiveObject),
        StructField::New("disabled", &InteractiveObject::disabled));
    InternalScript<InteractiveObject> interactiveObject("interactive_object",
        MetadataInteractiveObject,
        true,
        INTERACT_EVENT_INTERACT_POINT,
        INTERACT_EVENT_INTERACT_GRAB,
        INTERACT_EVENT_INTERACT_ROTATE);

    struct InteractHandler {
        float grabDistance = 2.0f;
        EntityRef noclipEntity;
        Entity grabEntity, pointEntity, pressEntity;
        PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;

        // TODO: Move this to a separate script on noclip target
        void UpdateGrabTarget(EntityLock<Write<PhysicsJoints>> lock, Entity newGrabEntity) {
            auto noclipEnt = noclipEntity.Get(lock);
            if (!noclipEnt.Has<PhysicsJoints>(lock)) return;
            // auto &joints = noclipEnt.Get<PhysicsJoints>(lock);
            PhysicsJoint joint;
            joint.type = PhysicsJointType::NoClip;
            joint.target = grabEntity;
            if (newGrabEntity == grabEntity) {
                // if (grabEntity) joints.Add(joint);
            } else {
                // auto it = std::find(joints.joints.begin(), joints.joints.end(), joint);
                // if (it != joints.joints.end()) {
                //     it->type = PhysicsJointType::TemporaryNoClip;
                // }
                // if (newGrabEntity) {
                //     joint.target = newGrabEntity;
                //     joints.Add(joint);
                // }
                grabEntity = newGrabEntity;
            }
        }

        void OnTick(ScriptState &state, EntityLock<WriteAll> entLock, chrono_clock::duration interval) {
            if (entLock.Has<TransformSnapshot, PhysicsQuery>()) {
                auto &query = entLock.Get<PhysicsQuery>();
                auto &transform = entLock.Get<TransformSnapshot>();

                PhysicsQuery::Raycast::Result raycastResult;
                if (raycastQuery) {
                    auto &result = query.Lookup(raycastQuery).result;
                    if (result) raycastResult = result.value();
                } else {
                    raycastQuery = query.NewQuery(PhysicsQuery::Raycast(grabDistance,
                        PhysicsGroupMask(
                            PHYSICS_GROUP_WORLD | PHYSICS_GROUP_INTERACTIVE | PHYSICS_GROUP_USER_INTERFACE)));
                }

                bool rotating = SignalBindings::GetSignal(entLock, "interact_rotate") >= 0.5;

                Event event;
                while (EventInput::Poll(entLock, state.eventQueue, event)) {
                    if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                        auto justDropped = grabEntity;
                        if (grabEntity) {
                            // Drop the currently held entity
                            EventBindings::SendEvent(entLock,
                                grabEntity,
                                Event{INTERACT_EVENT_INTERACT_GRAB, entLock.entity, false});
                            UpdateGrabTarget(entLock, {});
                        }
                        if (std::holds_alternative<bool>(event.data)) {
                            auto &grabEvent = std::get<bool>(event.data);
                            if (grabEvent && raycastResult.target && raycastResult.target != justDropped) {
                                // Grab the entity being looked at
                                if (EventBindings::SendEvent(entLock,
                                        raycastResult.target,
                                        Event{INTERACT_EVENT_INTERACT_GRAB, entLock.entity, transform}) > 0) {
                                    UpdateGrabTarget(entLock, raycastResult.target);
                                }
                            }
                        } else if (std::holds_alternative<Entity>(event.data)) {
                            auto &targetEnt = std::get<Entity>(event.data);
                            if (targetEnt) {
                                // Grab the entity requested by the event
                                if (EventBindings::SendEvent(entLock,
                                        targetEnt,
                                        Event{INTERACT_EVENT_INTERACT_GRAB, entLock.entity, transform}) > 0) {
                                    UpdateGrabTarget(entLock, targetEnt);
                                }
                            }
                        } else {
                            Errorf("Unsupported grab event type: %s", event.toString());
                        }
                    } else if (event.name == INTERACT_EVENT_INTERACT_PRESS) {
                        if (std::holds_alternative<bool>(event.data)) {
                            if (pressEntity) {
                                // Unpress the currently pressed entity
                                EventBindings::SendEvent(entLock,
                                    pressEntity,
                                    Event{INTERACT_EVENT_INTERACT_PRESS, entLock.entity, false});
                                pressEntity = {};
                            }
                            if (std::get<bool>(event.data) && raycastResult.target) {
                                // Press the entity being looked at
                                EventBindings::SendEvent(entLock,
                                    raycastResult.target,
                                    Event{INTERACT_EVENT_INTERACT_PRESS, entLock.entity, true});
                                pressEntity = raycastResult.target;
                            }
                        }
                    } else if (event.name == INTERACT_EVENT_INTERACT_ROTATE) {
                        if (rotating && grabEntity) {
                            EventBindings::SendEvent(entLock,
                                grabEntity,
                                Event{INTERACT_EVENT_INTERACT_ROTATE, entLock.entity, event.data});
                        }
                    }
                }

                if (pointEntity && raycastResult.target != pointEntity) {
                    EventBindings::SendEvent(entLock,
                        pointEntity,
                        Event{INTERACT_EVENT_INTERACT_POINT, entLock.entity, false});
                }
                if (raycastResult.target) {
                    Transform pointTransfrom = transform;
                    pointTransfrom.SetPosition(raycastResult.position);
                    EventBindings::SendEvent(entLock,
                        raycastResult.target,
                        Event{INTERACT_EVENT_INTERACT_POINT, entLock.entity, pointTransfrom});
                }
                pointEntity = raycastResult.target;
            }
        }
    };
    StructMetadata MetadataInteractHandler(typeid(InteractHandler),
        StructField::New("grab_distance", &InteractHandler::grabDistance),
        StructField::New("noclip_entity", &InteractHandler::noclipEntity));
    InternalScript<InteractHandler> interactHandler("interact_handler",
        MetadataInteractHandler,
        false,
        INTERACT_EVENT_INTERACT_GRAB,
        INTERACT_EVENT_INTERACT_PRESS,
        INTERACT_EVENT_INTERACT_ROTATE);
} // namespace sp::scripts
