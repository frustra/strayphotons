#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

#include <glm/gtx/component_wise.hpp>
#include <unordered_set>

namespace sp::scripts {
    using namespace ecs;

    std::array magnetScripts = {
        InternalScript(
            "magnetic_plug",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (!ent.Has<PhysicsJoints>(lock)) return;
                auto &joints = ent.Get<PhysicsJoints>(lock);

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

                                if (scriptData.grabEntities.empty() && !scriptData.socketEntities.empty()) {
                                    // Attach to a nearby socket if one exists
                                    // TODO: Sort by proximity when multiple are nearby
                                    scriptData.attachedEntity = *scriptData.socketEntities.begin();

                                    PhysicsJoint joint;
                                    joint.target = scriptData.attachedEntity;
                                    joint.type = PhysicsJointType::Fixed;
                                    joints.Add(joint);
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
