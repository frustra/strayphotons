/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/EnumTypes.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "input/BindingNames.hh"

namespace sp::scripts {
    using namespace ecs;
    static CVar<int> CVarHandCollisionShapes("p.HandCollisionShapes", 1, "0: boxes, 1: capsules");
    static CVar<int> CVarHandOverlapTest("p.HandOverlapTest",
        0,
        "0: no overlap test, 1: per-finger overlap, 2: whole-hand overlap");

    enum class BoneGroup {
        Wrist = 0,
        Thumb,
        Index,
        Middle,
        Ring,
        Pinky,
    };

    struct BoneProperties {
        std::string boneName;
        BoneGroup group;
        float radius;
        glm::vec3 offset;
    };

    static const std::array boneDefinitions = {
        BoneProperties{"wrist_#", BoneGroup::Wrist, -1.0f, glm::vec3(0.01, 0.0, 0.01)},

        BoneProperties{"finger_thumb_0_#", BoneGroup::Thumb, 0.015f},
        BoneProperties{"finger_thumb_1_#", BoneGroup::Thumb, 0.01f},
        BoneProperties{"finger_thumb_2_#", BoneGroup::Thumb, 0.01f},
        BoneProperties{"finger_thumb_#_end", BoneGroup::Thumb, 0.008f},

        BoneProperties{"finger_index_meta_#", BoneGroup::Index, 0.015f},
        BoneProperties{"finger_index_0_#", BoneGroup::Index, 0.015f},
        BoneProperties{"finger_index_1_#", BoneGroup::Index, 0.01f},
        BoneProperties{"finger_index_2_#", BoneGroup::Index, 0.01f},
        BoneProperties{"finger_index_#_end", BoneGroup::Index, 0.008f},

        BoneProperties{"finger_middle_meta_#", BoneGroup::Middle, 0.015f},
        BoneProperties{"finger_middle_0_#", BoneGroup::Middle, 0.015f},
        BoneProperties{"finger_middle_1_#", BoneGroup::Middle, 0.01f},
        BoneProperties{"finger_middle_2_#", BoneGroup::Middle, 0.01f},
        BoneProperties{"finger_middle_#_end", BoneGroup::Middle, 0.008f},

        BoneProperties{"finger_ring_meta_#", BoneGroup::Ring, 0.015f},
        BoneProperties{"finger_ring_0_#", BoneGroup::Ring, 0.015f},
        BoneProperties{"finger_ring_1_#", BoneGroup::Ring, 0.01f},
        BoneProperties{"finger_ring_2_#", BoneGroup::Ring, 0.01f},
        BoneProperties{"finger_ring_#_end", BoneGroup::Ring, 0.008f},

        BoneProperties{"finger_pinky_meta_#", BoneGroup::Pinky, 0.015f},
        BoneProperties{"finger_pinky_0_#", BoneGroup::Pinky, 0.015f},
        BoneProperties{"finger_pinky_1_#", BoneGroup::Pinky, 0.01f},
        BoneProperties{"finger_pinky_2_#", BoneGroup::Pinky, 0.01f},
        BoneProperties{"finger_pinky_#_end", BoneGroup::Pinky, 0.008f},
    };

    struct VrHandScript {
        // Input parameters
        std::string handStr;
        EntityRef noclipEntity;
        float teleportDistance = 2.0f;
        float pointDistance = 2.0f;
        float forceLimit = 100.0f;
        float torqueLimit = 10.0f;

        // Internal state
        bool init = false;
        std::array<EntityRef, boneDefinitions.size()> inputRefs;
        std::array<EntityRef, boneDefinitions.size()> physicsRefs;
        std::array<PhysicsQuery::Handle<PhysicsQuery::Overlap>, boneDefinitions.size()> queries;
        std::array<Transform, boneDefinitions.size()> queryTransforms;
        std::array<PhysicsShape, boneDefinitions.size()> currentShapes;
        PhysicsGroupMask collisionMask;
        EntityRef inputRootRef, physicsRootRef, controllerRef, laserPointerRef;
        Entity grabEntity, pointEntity, pressEntity;
        std::string actionPrefix;
        SignalRef indexCurlRef, middleCurlRef;

        PhysicsQuery::Handle<PhysicsQuery::Raycast> pointQueryHandle;

        bool Init(ScriptState &state, Lock<Read<ecs::Name>> lock, const Entity &ent) {
            laserPointerRef = ecs::Name("vr", "laser_pointer");

            char handChar = 'l';
            ecs::Name inputScope("input", "");
            if (sp::iequals(handStr, "left")) {
                handChar = 'l';
                inputScope.entity = "vr_actions_main_in_lefthand_anim";
                controllerRef = ecs::Name("vr", "controller_left");
                actionPrefix = "actions_main_in_lefthand_anim";
                collisionMask = (PhysicsGroupMask)(ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_PLAYER_RIGHT_HAND |
                                                   ecs::PHYSICS_GROUP_HELD_OBJECT | ecs::PHYSICS_GROUP_USER_INTERFACE);
            } else if (sp::iequals(handStr, "right")) {
                handChar = 'r';
                inputScope.entity = "vr_actions_main_in_righthand_anim";
                controllerRef = ecs::Name("vr", "controller_right");
                actionPrefix = "actions_main_in_righthand_anim";
                collisionMask = (PhysicsGroupMask)(ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_PLAYER_LEFT_HAND |
                                                   ecs::PHYSICS_GROUP_HELD_OBJECT | ecs::PHYSICS_GROUP_USER_INTERFACE);
            } else {
                Errorf("Invalid hand specified for VrHand script: %s", handStr);
                return false;
            }
            indexCurlRef = SignalRef(controllerRef, actionPrefix + "_curl_index");
            middleCurlRef = SignalRef(controllerRef, actionPrefix + "_curl_middle");
            inputRootRef = inputScope;
            if (!inputRootRef) {
                Errorf("VrHand script has invalid input root: %s", inputScope.String());
                return false;
            }

            auto &physicsScope = ent.Get<Name>(lock);
            physicsRootRef = physicsScope;
            if (!physicsRootRef) {
                Errorf("VrHand script has invalid physics root: %s", physicsScope.String());
                return false;
            }

            for (size_t i = 0; i < boneDefinitions.size(); i++) {
                std::string name(boneDefinitions[i].boneName);
                std::transform(name.begin(), name.end(), name.begin(), [&](unsigned char c) {
                    return c == '#' ? handChar : c;
                });

                inputRefs[i] = ecs::Name(name, inputScope);
                physicsRefs[i] = ecs::Name(name, physicsScope);

                if (!inputRefs[i]) {
                    Errorf("VrHand has invalid input entity: %s with scope %s", name, inputScope.String());
                }
                if (!physicsRefs[i]) {
                    Errorf("VrHand has invalid physics entity: %s with scope %s", name, physicsScope.String());
                }
            }
            return true;
        }

        PhysicsShape ShapeForBone(Lock<Read<TransformTree>> lock, const Entity &inputRoot, size_t index) const {
            auto inputEnt = inputRefs[index].Get(lock);
            if (!inputEnt.Has<TransformTree>(lock)) return PhysicsShape();

            auto &segment = boneDefinitions[index];
            auto &bone = inputEnt.Get<TransformTree>(lock);

            auto relativeTransform = bone.GetRelativeTransform(lock, inputRoot);
            PhysicsShape shape;
            shape.transform = relativeTransform;
            shape.transform.Translate(shape.transform.GetRotation() * segment.offset);

            if (segment.radius < 0.0f) {
                shape.shape = PhysicsShape::Box(glm::vec3(0.04, 0.07, 0.06));
                return shape;
            }

            auto parentEntity = bone.parent.Get(lock);
            if (!parentEntity.Has<TransformTree>(lock)) {
                shape.shape = PhysicsShape::Sphere(segment.radius);
                return shape;
            }
            auto parentTransform = parentEntity.Get<const TransformTree>(lock).GetRelativeTransform(lock, inputRoot);

            float boneLength = glm::length(bone.pose.GetPosition());
            if (boneLength <= 1e-5f) {
                shape.shape = PhysicsShape::Sphere(segment.radius);
                return shape;
            }

            if (CVarHandCollisionShapes.Get() == 0) {
                shape.shape = PhysicsShape::Box(glm::vec3(boneLength, segment.radius, segment.radius));
            } else {
                shape.shape = PhysicsShape::Capsule(boneLength, segment.radius);
            }

            auto boneDiff = parentTransform.GetPosition() - relativeTransform.GetPosition();
            glm::vec3 boneVector = glm::vec4(boneDiff, 0.0f);
            // Place the center of the capsule halfway between this bone and its parent
            shape.transform.Translate(boneVector * 0.5f);
            // Rotate the capsule so it's aligned with the bone vector
            shape.transform.SetRotation(glm::quat(glm::vec3(1, 0, 0), boneVector));
            return shape;
        };

        void HandlePointing(ScriptState &state, PhysicsUpdateLock lock, const Entity &ent, bool isPointing) {
            glm::vec3 pointOrigin, pointDir, pointPos;
            Entity pointTarget;

            auto &query = ent.Get<PhysicsQuery>(lock);
            if (isPointing) {
                if (physicsRefs[6] && physicsRefs[7]) {
                    auto indexBone0 = physicsRefs[6].Get(lock);
                    auto indexBone1 = physicsRefs[7].Get(lock);

                    if (indexBone0.Has<TransformSnapshot>(lock) && indexBone1.Has<TransformSnapshot>(lock)) {
                        auto &tr0 = indexBone0.Get<TransformSnapshot>(lock).globalPose;
                        auto &tr1 = indexBone1.Get<TransformSnapshot>(lock).globalPose;

                        pointOrigin = tr1.GetPosition();
                        pointDir = glm::normalize(pointOrigin - tr0.GetPosition());
                    }
                }

                if (!pointQueryHandle) {
                    pointQueryHandle = query.NewQuery(
                        PhysicsQuery::Raycast(pointDistance, PhysicsGroupMask(PHYSICS_GROUP_USER_INTERFACE)));
                }

                auto &pointQuery = query.Lookup(pointQueryHandle);

                if (glm::length(pointDir) > 0) {
                    pointQuery.direction = pointDir;
                    pointQuery.relativeDirection = false;
                    pointQuery.position = pointOrigin;
                    pointQuery.relativePosition = false;
                }

                if (pointQuery.result) {
                    auto pointResult = pointQuery.result.value();
                    pointTarget = pointResult.target;
                    pointPos = pointResult.position;
                }
            }

            if (pointEntity && pointEntity != pointTarget) {
                EventBindings::SendEvent(lock, pointEntity, Event{INTERACT_EVENT_INTERACT_POINT, ent, false});
            }
            if (pointTarget) {
                Transform pointTransform;
                pointTransform.SetPosition(pointPos);
                EventBindings::SendEvent(lock, pointTarget, Event{INTERACT_EVENT_INTERACT_POINT, ent, pointTransform});
            }
            pointEntity = pointTarget;

            auto laser = laserPointerRef.Get(lock);
            if (laser && laser.Has<LaserLine>(lock)) {
                auto &laserLine = laser.Get<LaserLine>(lock);
                if (pointTarget && std::holds_alternative<LaserLine::Line>(laserLine.line)) {
                    auto &line = std::get<LaserLine::Line>(laserLine.line);
                    if (line.points.size() != 2) line.points.resize(2);
                    line.points[0] = pointOrigin;
                    line.points[1] = pointPos;
                    laserLine.on = true;
                } else {
                    laserLine.on = false;
                }
            }
        }

        void UpdateGrabTarget(Lock<Write<PhysicsJoints>> lock, Entity newGrabEntity) {
            auto noclipEnt = noclipEntity.Get(lock);
            if (!noclipEnt.Has<PhysicsJoints>(lock)) return;
            auto &joints = noclipEnt.Get<PhysicsJoints>(lock);
            PhysicsJoint joint;
            joint.type = PhysicsJointType::NoClip;
            joint.target = grabEntity;
            if (newGrabEntity == grabEntity) {
                joints.Add(joint);
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

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            ZoneScopedN("VrHandScript");
            if (!ent.Has<Name, Physics, PhysicsJoints, PhysicsQuery, TransformTree>(lock)) return;

            if (!init) {
                if (!Init(state, lock, ent)) {
                    // Don't run again
                    state.definition.events.clear();
                    state.definition.filterOnEvent = true;
                    return;
                }
                init = true;
            }

            auto &ph = ent.Get<Physics>(lock);
            auto &joints = ent.Get<PhysicsJoints>(lock).joints;
            auto &query = ent.Get<PhysicsQuery>(lock);
            auto &transform = ent.Get<TransformTree>(lock);
            auto inputRoot = inputRootRef.Get(lock);

            // Read and update overlap queries
            EnumArray<Entity, BoneGroup> groupOverlaps = {};
            for (size_t i = 0; i < boneDefinitions.size(); i++) {
                if (!inputRefs[i] || !physicsRefs[i]) continue;

                auto inputEnt = inputRefs[i].Get(lock);
                if (inputEnt.Has<TransformTree>(lock)) {
                    auto boneShape = ShapeForBone(lock, inputRoot, i);
                    if (!queries[i]) {
                        queries[i] = query.NewQuery(PhysicsQuery::Overlap{boneShape, collisionMask});
                    } else {
                        auto &overlapQuery = query.Lookup(queries[i]);
                        overlapQuery.shape = boneShape;
                        overlapQuery.filterGroup = collisionMask;

                        if (overlapQuery.result && *overlapQuery.result) {
                            auto group = CVarHandOverlapTest.Get() == 2 ? BoneGroup::Wrist : boneDefinitions[i].group;
                            groupOverlaps[group] = *overlapQuery.result;
                        }
                    }

                    auto &inputTransform = inputEnt.Get<TransformTree>(lock);
                    queryTransforms[i] = inputTransform.GetRelativeTransform(lock, inputRoot);
                }
            }

            bool forceTeleport = false;
            auto constraintTarget = inputRootRef.Get(lock);
            if (constraintTarget.Has<TransformTree>(lock)) {
                auto &inputTransform = constraintTarget.Get<TransformTree>(lock);
                // Don't set the hand constraint target until the controller is valid
                if (inputTransform.parent) {
                    if (joints.empty()) joints.resize(1);
                    if (joints[0].target != constraintTarget || joints[0].type != PhysicsJointType::Force) {
                        joints[0].target = inputRootRef;
                        joints[0].type = PhysicsJointType::Force;
                        joints[0].limit = glm::vec2(forceLimit, torqueLimit);
                        forceTeleport = true;
                    }
                }
            }

            // Teleport the hands back to the player if they get too far away
            bool teleported = false;
            if (teleportDistance > 0 || forceTeleport) {
                if (constraintTarget.Has<TransformTree>(lock)) {
                    Assertf(!transform.parent, "vr_hand script transform can't have parent");
                    auto parentTransform = constraintTarget.Get<TransformTree>(lock).GetGlobalTransform(lock);

                    auto dist = glm::length(transform.pose.GetPosition() - parentTransform.GetPosition());
                    if (dist >= teleportDistance || forceTeleport) {
                        transform.pose = parentTransform;
                        teleported = true;
                    }
                }
            }

            // Handle interaction events
            auto indexCurl = indexCurlRef.GetSignal(lock);
            auto grabSignal = indexCurl;
            auto grabTarget = grabEntity;
            if (teleported || grabSignal < 0.18) {
                grabTarget = {};
            } else if (grabSignal > 0.2 && !grabTarget) {
                grabTarget = groupOverlaps[BoneGroup::Index];
            }

            auto middleCurl = middleCurlRef.GetSignal(lock);
            bool isPointing = indexCurl < 0.05 && middleCurl > 0.5;
            HandlePointing(state, lock, ent, isPointing);

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name == INTERACT_EVENT_INTERACT_PRESS) {
                    if (std::holds_alternative<bool>(event.data)) {
                        if (pressEntity) {
                            // Unpress the currently pressed entity
                            EventBindings::SendEvent(lock,
                                pressEntity,
                                Event{INTERACT_EVENT_INTERACT_PRESS, ent, false});
                            pressEntity = {};
                        }
                        if (std::get<bool>(event.data) && pointEntity) {
                            // Press the entity being looked at
                            EventBindings::SendEvent(lock,
                                pointEntity,
                                Event{INTERACT_EVENT_INTERACT_PRESS, ent, true});
                            pressEntity = pointEntity;
                        }
                    }
                } else if (event.name == INTERACT_EVENT_INTERACT_GRAB) {
                    if (!std::holds_alternative<Entity>(event.data)) continue;
                    grabTarget = std::get<Entity>(event.data);
                }
            }

            if (grabEntity && grabEntity != grabTarget) {
                // Drop the currently held entity
                EventBindings::SendEvent(lock, grabEntity, Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                UpdateGrabTarget(lock, {});
            }
            if (grabTarget && grabTarget != grabEntity) {
                // Grab the entity being overlapped
                auto globalTransform = transform.GetGlobalTransform(lock);
                if (EventBindings::SendEvent(lock,
                        grabTarget,
                        Event{INTERACT_EVENT_INTERACT_GRAB, ent, globalTransform}) > 0) {
                    UpdateGrabTarget(lock, grabTarget);
                }
            }

            // Update the hand's physics shape
            ph.shapes.clear();
            for (size_t i = 0; i < boneDefinitions.size(); i++) {
                if (!inputRefs[i] || !physicsRefs[i] || !queries[i]) continue;

                auto physicsEnt = physicsRefs[i].Get(lock);
                if (physicsEnt.Has<TransformTree>(lock)) {
                    auto group = CVarHandOverlapTest.Get() == 2 ? BoneGroup::Wrist : boneDefinitions[i].group;
                    if (!groupOverlaps[group] || CVarHandOverlapTest.Get() == 0) {
                        if (!grabEntity) {
                            // This group doesn't overlap, so update the current pose and shape
                            auto &overlapQuery = query.Lookup(queries[i]);
                            currentShapes[i] = overlapQuery.shape;

                            auto &physicsTransform = physicsEnt.Get<TransformTree>(lock);
                            physicsTransform.parent = physicsRootRef;
                            physicsTransform.pose = queryTransforms[i];
                        }
                    }
                    if (currentShapes[i]) ph.shapes.push_back(currentShapes[i]);
                }
            }
        }
    };
    StructMetadata MetadataVrHandScript(typeid(VrHandScript),
        "VrHandScript",
        "",
        StructField::New("hand", &VrHandScript::handStr),
        StructField::New("teleport_distance", &VrHandScript::teleportDistance),
        StructField::New("point_distance", &VrHandScript::pointDistance),
        StructField::New("force_limit", &VrHandScript::forceLimit),
        StructField::New("torque_limit", &VrHandScript::torqueLimit),
        StructField::New("noclip_entity", &VrHandScript::noclipEntity));
    InternalPhysicsScript<VrHandScript> vrHandScript("vr_hand",
        MetadataVrHandScript,
        false,
        INTERACT_EVENT_INTERACT_GRAB,
        INTERACT_EVENT_INTERACT_PRESS);
} // namespace sp::scripts
