#include "core/Common.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

#include <glm/gtx/component_wise.hpp>

namespace sp::scripts {
    using namespace ecs;

    static CVar<float> CVarMaxMagentForce("i.MaxMagnetForce", 1.0f, "Maximum force applied to magnetic objects");
    static CVar<float> CVarMaxMagentTorque("i.MaxMagnetTorque", 0.01f, "Maximum torque applied to magnetic objects");

    std::array magnetScripts = {
        InternalScript(
            "magnetic_plug",
            [](ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
                if (!ent.Has<PhysicsJoints>(lock)) return;
                auto &joints = ent.Get<PhysicsJoints>(lock);

                struct ScriptData {
                    Entity socketEntity;
                    std::vector<Entity> grabEntities;
                };
                ScriptData scriptData = {};
                if (state.userData.has_value()) scriptData = std::any_cast<ScriptData>(state.userData);

                Event event;
                while (EventInput::Poll(lock, state.eventQueue, event)) {
                    if (event.name == "/magnet/attach") {
                        PhysicsJoint joint;
                        joint.target = event.source;
                        joint.type = PhysicsJointType::Fixed;
                        // joint.limit = glm::vec2(CVarMaxMagentForce.Get(), CVarMaxMagentTorque.Get());
                        joints.Add(joint);

                        scriptData.socketEntity = event.source;

                        for (auto &grabEntity : scriptData.grabEntities) {
                            EventBindings::SendEvent(lock, grabEntity, Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                        }
                    } else if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                        if (std::holds_alternative<bool>(event.data)) {
                            // Grab(false) = Drop
                            sp::erase(scriptData.grabEntities, event.source);
                        } else if (std::holds_alternative<Transform>(event.data)) {
                            if (scriptData.socketEntity) {
                                Debugf("Detached: %s from %s",
                                    ecs::ToString(lock, ent),
                                    ecs::ToString(lock, scriptData.socketEntity));

                                sp::erase_if(joints.joints, [&](auto &&joint) {
                                    return joint.target == scriptData.socketEntity;
                                });
                            }

                            scriptData.grabEntities.emplace_back(event.source);
                        } else {
                            Errorf("Unsupported grab event type: %s", event.toString());
                        }
                    }
                }

                state.userData = scriptData;
            },
            "/magnet/attach",
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
                        }
                    } else if (event.name == "/trigger/magnetic/enter") {
                        if (event.source == ent && !scriptData.disabledEntities.contains(*data)) {
                            Debugf("Attached: %s to %s", ecs::ToString(lock, *data), ecs::ToString(lock, event.source));
                            scriptData.disabledEntities.emplace(*data);
                            EventBindings::SendEvent(lock, *data, Event{"/magnet/attach", ent, true});
                        }
                    }
                }

                state.userData = scriptData;
            },
            "/trigger/magnetic/enter",
            "/trigger/magnetic/leave"),
    };
} // namespace sp::scripts
