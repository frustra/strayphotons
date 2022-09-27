#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"

#include <glm/glm.hpp>

namespace sp::scripts {
    using namespace ecs;

    static CVar<float> CVarMaxGrabForce("i.MaxGrabForce", 20.0f, "Maximum force applied to held objects");
    static CVar<float> CVarMaxGrabTorque("i.MaxGrabTorque", 10.0f, "Maximum torque applied to held objects");

    std::array interactionScripts = {
        InternalScript("interactive_object",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput, TransformSnapshot, Physics, PhysicsJoints>(lock)) {
                    auto &ph = ent.Get<Physics>(lock);
                    auto &joints = ent.Get<PhysicsJoints>(lock);
                    if (!ph.dynamic || ph.kinematic) {
                        Errorf("Interactive object must have dynamic physics: %s", ToString(lock, ent));
                        return;
                    }

                    struct ScriptData {
                        std::vector<Entity> grabEntities, pointEntities;
                        Transform pointTransform;
                        bool renderOutline = false;
                        PhysicsQuery::Handle<PhysicsQuery::Mass> massQuery;
                    };

                    ScriptData scriptData;
                    if (state.userData.has_value()) scriptData = std::any_cast<ScriptData>(state.userData);

                    glm::vec3 centerOfMass;
                    if (ent.Has<PhysicsQuery>(lock)) {
                        auto &query = ent.Get<PhysicsQuery>(lock);
                        if (scriptData.massQuery) {
                            auto &result = query.Lookup(scriptData.massQuery).result;
                            if (result) centerOfMass = result->centerOfMass;
                        } else {
                            scriptData.massQuery = query.NewQuery(PhysicsQuery::Mass(ent));
                        }
                    }

                    Event event;
                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_POINT, event)) {
                        auto pointTransform = std::get_if<Transform>(&event.data);
                        if (pointTransform) {
                            scriptData.pointTransform = *pointTransform;
                            scriptData.pointEntities.emplace_back(event.source);
                        } else if (std::holds_alternative<bool>(event.data)) {
                            scriptData.pointTransform = {};
                            sp::erase(scriptData.pointEntities, event.source);
                        } else {
                            Errorf("Unsupported point event type: %s", event.toString());
                        }
                    }

                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_GRAB, event)) {
                        if (std::holds_alternative<bool>(event.data)) {
                            // Grab(false) = Drop
                            sp::erase_if(joints.joints, [&](auto &&joint) {
                                return joint.target == event.source;
                            });
                            sp::erase(scriptData.grabEntities, event.source);
                        } else if (std::holds_alternative<Transform>(event.data)) {
                            auto &parentTransform = std::get<Transform>(event.data);
                            auto &transform = ent.Get<TransformSnapshot>(lock);
                            auto invParentRotate = glm::inverse(parentTransform.GetRotation());

                            scriptData.grabEntities.emplace_back(event.source);

                            static CVar<bool> includeFixed("x.includeFixed", true, "");
                            PhysicsJoint joint;
                            joint.target = event.source;
                            joint.type = PhysicsJointType::Fixed;
                            joint.remoteOffset = invParentRotate *
                                                 (transform.GetPosition() - parentTransform.GetPosition());
                            joint.remoteOrient = invParentRotate * transform.GetRotation();
                            if (includeFixed.Get()) joints.Add(joint);
                            joint.type = PhysicsJointType::Force;
                            // TODO: Read this property from player
                            joint.limit = glm::vec2(CVarMaxGrabForce.Get(), CVarMaxGrabTorque.Get());
                            joints.Add(joint);
                        } else {
                            Errorf("Unsupported grab event type: %s", event.toString());
                        }
                    }

                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_ROTATE, event)) {
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
                                    auto center = joint.remoteOrient * centerOfMass;
                                    joint.remoteOffset += center - (deltaRotate * center);
                                    joint.remoteOrient = deltaRotate * joint.remoteOrient;
                                }
                            }
                        }
                    }

                    bool renderOutline = !scriptData.grabEntities.empty() || !scriptData.pointEntities.empty();
                    if (scriptData.renderOutline != renderOutline) {
                        for (auto &e : lock.EntitiesWith<Renderable>()) {
                            if (!e.Has<TransformTree, Renderable>(lock)) continue;

                            auto child = e;
                            while (child.Has<TransformTree>(lock)) {
                                if (child == ent) {
                                    auto &visibility = e.Get<Renderable>(lock).visibility;
                                    if (renderOutline) {
                                        visibility |= VisibilityMask::OutlineSelection;
                                    } else {
                                        visibility &= ~VisibilityMask::OutlineSelection;
                                    }
                                    break;
                                }
                                child = child.Get<TransformTree>(lock).parent.Get(lock);
                            }
                        }
                        scriptData.renderOutline = renderOutline;
                    }

                    state.userData = scriptData;
                }
            }),
        InternalScript("interact_handler",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<EventInput, TransformSnapshot, PhysicsQuery>(lock)) {
                    auto &query = ent.Get<PhysicsQuery>(lock);
                    auto &transform = ent.Get<TransformSnapshot>(lock);

                    struct ScriptData {
                        Entity grabEntity, pointEntity, pressEntity;
                        PhysicsQuery::Handle<PhysicsQuery::Raycast> raycastQuery;
                    };

                    ScriptData scriptData;
                    if (state.userData.has_value()) scriptData = std::any_cast<ScriptData>(state.userData);

                    PhysicsQuery::Raycast::Result raycastResult;
                    if (scriptData.raycastQuery) {
                        auto &result = query.Lookup(scriptData.raycastQuery).result;
                        if (result) raycastResult = result.value();
                    } else {
                        auto grabDistance = state.GetParam<double>("grab_distance");
                        scriptData.raycastQuery = query.NewQuery(PhysicsQuery::Raycast(grabDistance,
                            PhysicsGroupMask(
                                PHYSICS_GROUP_WORLD | PHYSICS_GROUP_INTERACTIVE | PHYSICS_GROUP_USER_INTERFACE)));
                    }

                    Event event;
                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_GRAB, event)) {
                        if (std::holds_alternative<bool>(event.data)) {
                            auto &grabEvent = std::get<bool>(event.data);
                            auto justDropped = scriptData.grabEntity;
                            if (scriptData.grabEntity) {
                                // Drop the currently held entity
                                EventBindings::SendEvent(lock,
                                    scriptData.grabEntity,
                                    INTERACT_EVENT_INTERACT_GRAB,
                                    Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                                scriptData.grabEntity = {};
                            }
                            if (grabEvent && raycastResult.target && raycastResult.target != justDropped) {
                                // Grab the entity being looked at
                                if (EventBindings::SendEvent(lock,
                                        raycastResult.target,
                                        INTERACT_EVENT_INTERACT_GRAB,
                                        Event{INTERACT_EVENT_INTERACT_GRAB, ent, transform}) > 0) {
                                    scriptData.grabEntity = raycastResult.target;
                                }
                            }
                        } else {
                            Errorf("Unsupported grab event type: %s", event.toString());
                        }
                    }

                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_PRESS, event)) {
                        if (std::holds_alternative<bool>(event.data)) {
                            if (scriptData.pressEntity) {
                                // Unpress the currently pressed entity
                                EventBindings::SendEvent(lock,
                                    scriptData.pressEntity,
                                    INTERACT_EVENT_INTERACT_PRESS,
                                    Event{INTERACT_EVENT_INTERACT_PRESS, ent, false});
                                scriptData.pressEntity = {};
                            }
                            if (std::get<bool>(event.data) && raycastResult.target) {
                                // Press the entity being looked at
                                EventBindings::SendEvent(lock,
                                    raycastResult.target,
                                    INTERACT_EVENT_INTERACT_PRESS,
                                    Event{INTERACT_EVENT_INTERACT_PRESS, ent, true});
                                scriptData.pressEntity = raycastResult.target;
                            }
                        }
                    }

                    bool rotating = SignalBindings::GetSignal(lock, ent, "interact_rotate") >= 0.5;
                    while (EventInput::Poll(lock, ent, INTERACT_EVENT_INTERACT_ROTATE, event)) {
                        if (rotating && scriptData.grabEntity) {
                            EventBindings::SendEvent(lock,
                                scriptData.grabEntity,
                                INTERACT_EVENT_INTERACT_ROTATE,
                                Event{INTERACT_EVENT_INTERACT_ROTATE, ent, event.data});
                        }
                    }

                    if (scriptData.pointEntity && raycastResult.target != scriptData.pointEntity) {
                        EventBindings::SendEvent(lock,
                            scriptData.pointEntity,
                            INTERACT_EVENT_INTERACT_POINT,
                            Event{INTERACT_EVENT_INTERACT_POINT, ent, false});
                    }
                    if (raycastResult.target) {
                        Transform pointTransfrom = transform;
                        pointTransfrom.SetPosition(raycastResult.position);
                        EventBindings::SendEvent(lock,
                            raycastResult.target,
                            INTERACT_EVENT_INTERACT_POINT,
                            Event{INTERACT_EVENT_INTERACT_POINT, ent, pointTransfrom});
                    }
                    scriptData.pointEntity = raycastResult.target;

                    state.userData = scriptData;
                }
            }),
    };
} // namespace sp::scripts
