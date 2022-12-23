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
        std::vector<Entity> grabEntities, pointEntities;
        bool renderOutline = false;
        PhysicsQuery::Handle<PhysicsQuery::Mass> massQuery;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformSnapshot, Physics, PhysicsJoints>(lock)) return;

            auto &ph = ent.Get<Physics>(lock);
            auto &joints = ent.Get<PhysicsJoints>(lock);
            if (!ph.dynamic || ph.kinematic) {
                Errorf("Interactive object must have dynamic physics: %s", ToString(lock, ent));
                return;
            }

            glm::vec3 centerOfMass;
            if (ent.Has<PhysicsQuery>(lock)) {
                auto &query = ent.Get<PhysicsQuery>(lock);
                if (massQuery) {
                    auto &result = query.Lookup(massQuery).result;
                    if (result) centerOfMass = result->centerOfMass;
                } else {
                    massQuery = query.NewQuery(PhysicsQuery::Mass(ent));
                }
            }

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
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
                        sp::erase_if(joints.joints, [&](auto &&joint) {
                            return joint.target == event.source;
                        });
                        sp::erase(grabEntities, event.source);
                    } else if (std::holds_alternative<Transform>(event.data)) {
                        auto &parentTransform = std::get<Transform>(event.data);
                        auto &transform = ent.Get<TransformSnapshot>(lock);
                        auto invParentRotate = glm::inverse(parentTransform.GetRotation());

                        grabEntities.emplace_back(event.source);

                        PhysicsJoint joint;
                        joint.target = event.source;
                        if (CVarFixedJointGrab.Get()) {
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
                        if (!event.source.Has<TransformSnapshot>(lock)) continue;

                        auto &input = std::get<glm::vec2>(event.data);
                        auto &transform = event.source.Get<const TransformSnapshot>(lock);

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

            bool newRenderOutline = !grabEntities.empty() || !pointEntities.empty();
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
        }
    };
    StructMetadata MetadataInteractiveObject(typeid(InteractiveObject));
    InternalScript2<InteractiveObject> interactiveObject("interactive_object",
        MetadataInteractiveObject,
        true,
        INTERACT_EVENT_INTERACT_POINT,
        INTERACT_EVENT_INTERACT_GRAB,
        INTERACT_EVENT_INTERACT_ROTATE);

    struct InteractHandler {
        float grabDistance = 2.0f;
        Entity grabEntity, pointEntity, pressEntity;
        PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (ent.Has<TransformSnapshot, PhysicsQuery>(lock)) {
                auto &query = ent.Get<PhysicsQuery>(lock);
                auto &transform = ent.Get<TransformSnapshot>(lock);

                PhysicsQuery::Raycast::Result raycastResult;
                if (raycastQuery) {
                    auto &result = query.Lookup(raycastQuery).result;
                    if (result) raycastResult = result.value();
                } else {
                    raycastQuery = query.NewQuery(PhysicsQuery::Raycast(grabDistance,
                        PhysicsGroupMask(
                            PHYSICS_GROUP_WORLD | PHYSICS_GROUP_INTERACTIVE | PHYSICS_GROUP_USER_INTERFACE)));
                }

                bool rotating = SignalBindings::GetSignal(lock, ent, "interact_rotate") >= 0.5;

                Event event;
                while (EventInput::Poll(lock, state.eventQueue, event)) {
                    if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                        if (std::holds_alternative<bool>(event.data)) {
                            auto &grabEvent = std::get<bool>(event.data);
                            auto justDropped = grabEntity;
                            if (grabEntity) {
                                // Drop the currently held entity
                                EventBindings::SendEvent(lock,
                                    grabEntity,
                                    Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                                grabEntity = {};
                            }
                            if (grabEvent && raycastResult.target && raycastResult.target != justDropped) {
                                // Grab the entity being looked at
                                if (EventBindings::SendEvent(lock,
                                        raycastResult.target,
                                        Event{INTERACT_EVENT_INTERACT_GRAB, ent, transform}) > 0) {
                                    grabEntity = raycastResult.target;
                                }
                            }
                        } else {
                            Errorf("Unsupported grab event type: %s", event.toString());
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
        StructField::New("grab_distance", &InteractHandler::grabDistance));
    InternalScript2<InteractHandler> interactHandler("interact_handler",
        MetadataInteractHandler,
        false,
        INTERACT_EVENT_INTERACT_GRAB,
        INTERACT_EVENT_INTERACT_PRESS,
        INTERACT_EVENT_INTERACT_ROTATE);
} // namespace sp::scripts
