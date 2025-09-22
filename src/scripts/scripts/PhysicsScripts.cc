/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/ScriptImpl.hh"
#include "game/Scene.hh"

namespace sp::scripts {
    using namespace ecs;

    struct PhysicsJointFromEvent {
        robin_hood::unordered_map<std::string, PhysicsJoint> definedJoints;

        void Init(ScriptState &state) {
            state.definition.events.clear();
            state.definition.events.reserve(definedJoints.size() * 5);
            for (auto &[name, _] : definedJoints) {
                state.definition.events.emplace_back("/physics_joint/" + name + "/enable"); // bool
                state.definition.events.emplace_back("/physics_joint/" + name + "/set_target"); // EntityRef
                state.definition.events.emplace_back("/physics_joint/" + name + "/set_current_offset"); // EntityRef
                state.definition.events.emplace_back("/physics_joint/" + name + "/set_local_offset"); // Transform
                state.definition.events.emplace_back("/physics_joint/" + name + "/set_remote_offset"); // Transform
            }
            state.definition.filterOnEvent = true; // Effective next tick, only run when events arrive.
        }

        void OnTick(ScriptState &state,
            Lock<Write<PhysicsJoints>, Read<TransformSnapshot, EventInput>> lock,
            Entity ent,
            chrono_clock::duration interval) {
            if (!ent.Has<Physics, PhysicsJoints>(lock)) return;

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                Assertf(sp::starts_with(event.name, "/physics_joint/"),
                    "Event name should be /physics_joint/<name>/<action>");
                auto eventName = std::string_view(event.name).substr("/physics_joint/"s.size());
                auto delimiter = eventName.find('/');
                Assertf(delimiter != std::string_view::npos, "Event name should be /physics_joint/<name>/<action>");
                std::string jointName(eventName.substr(0, delimiter));
                auto action = eventName.substr(delimiter + 1);
                if (jointName.empty()) continue;

                auto it = definedJoints.find(jointName);
                if (it == definedJoints.end()) continue;
                auto &joint = it->second;
                auto &joints = ent.Get<PhysicsJoints>(lock).joints;
                auto existing = std::find_if(joints.begin(), joints.end(), [&joint](auto &arg) {
                    return arg.target == joint.target && arg.type == joint.type;
                });

                if (action == "enable") {
                    bool enabled = EventData::Visit(event.data, [](auto &data) {
                        using T = std::decay_t<decltype(data)>;

                        if constexpr (std::is_convertible_v<double, T> && std::is_convertible_v<T, double>) {
                            return (double)data >= 0.5;
                        } else if constexpr (std::is_convertible_v<T, bool>) {
                            return (bool)data;
                        } else {
                            return true;
                        }
                    });
                    if (existing == joints.end()) {
                        if (enabled) existing = joints.insert(joints.end(), joint);
                    } else if (!enabled) {
                        joints.erase(existing);
                        existing = joints.end();
                    }
                } else if (action == "set_target") {
                    if (event.data.type == EventDataType::String) {
                        auto &targetName = event.data.str;
                        joint.target = Name(targetName, Name());
                    } else if (event.data.type == EventDataType::NamedEntity) {
                        joint.target = event.data.namedEntity;
                    } else {
                        Errorf("Invalid set_target event type: %s", event.ToString());
                        continue;
                    }
                } else if (action == "set_current_offset") {
                    joint.localOffset = Transform();
                    Entity target;
                    if (event.data.type == EventDataType::String) {
                        auto &targetName = event.data.str;
                        target = EntityRef(Name(targetName, state.scope)).Get(lock);
                        if (!target) {
                            Errorf("Invalid set_current_offset event target: %s", targetName);
                        }
                    } else if (event.data.type == EventDataType::NamedEntity) {
                        auto &targetRef = event.data.namedEntity;
                        target = targetRef.Get(lock);
                        if (!target) {
                            Errorf("Invalid set_current_offset event target: %s", targetRef.Name().String());
                        }
                    } else {
                        Errorf("Invalid set_current_offset event type: %s", event.ToString());
                        continue;
                    }
                    if (ent.Has<TransformSnapshot>(lock) && target.Has<TransformSnapshot>(lock)) {
                        joint.localOffset = ent.Get<TransformSnapshot>(lock).globalPose.GetInverse() *
                                            target.Get<TransformSnapshot>(lock);
                    }
                } else if (action == "set_local_offset") {
                    if (event.data.type == EventDataType::Transform) {
                        joint.localOffset = event.data.transform;
                    } else {
                        Errorf("Invalid set_local_offset event type: %s", event.ToString());
                        continue;
                    }
                } else if (action == "set_remote_offset") {
                    if (event.data.type == EventDataType::Transform) {
                        joint.remoteOffset = event.data.transform;
                    } else {
                        Errorf("Invalid set_remote_offset event type: %s", event.ToString());
                        continue;
                    }
                } else {
                    Errorf("Unknown signal action: '%s'", std::string(action));
                }
                if (existing != joints.end()) {
                    *existing = joint;
                }
            }
        }
    };
    StructMetadata MetadataPhysicsJointFromEvent(typeid(PhysicsJointFromEvent),
        sizeof(PhysicsJointFromEvent),
        "PhysicsJointFromEvent",
        "",
        StructField::New(&PhysicsJointFromEvent::definedJoints));
    PhysicsScript<PhysicsJointFromEvent> physicsJointFromEvent("physics_joint_from_event",
        MetadataPhysicsJointFromEvent);
} // namespace sp::scripts
