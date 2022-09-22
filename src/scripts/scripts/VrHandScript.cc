#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
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
        Count,
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

    struct ScriptData {
        std::array<EntityRef, boneDefinitions.size()> inputRefs;
        std::array<EntityRef, boneDefinitions.size()> physicsRefs;
        std::array<PhysicsQuery::Handle<PhysicsQuery::Overlap>, boneDefinitions.size()> queries;
        std::array<Transform, boneDefinitions.size()> queryTransforms;
        std::array<PhysicsShape, boneDefinitions.size()> currentShapes;
        PhysicsGroupMask collisionMask;
        EntityRef inputRootRef, physicsRootRef, controllerRef, laserPointerRef;
        Entity grabEntity, pointEntity, pressEntity;
        std::string actionPrefix;

        PhysicsQuery::Handle<PhysicsQuery::Raycast> pointQuery;

        bool Init(ScriptState &state, Lock<Read<ecs::Name>> lock, Entity ent) {
            auto handStr = state.GetParam<std::string>("hand");
            to_lower(handStr);

            laserPointerRef = ecs::Name("vr", "laser_pointer");

            char handChar = 'l';
            ecs::Name inputScope("input", "");
            if (handStr == "left") {
                handChar = 'l';
                inputScope.entity = "vr_actions_main_in_lefthand_anim";
                controllerRef = ecs::Name("vr", "controller_left");
                actionPrefix = "actions_main_in_lefthand_anim";
                collisionMask = (PhysicsGroupMask)(ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_WORLD_OVERLAP |
                                                   ecs::PHYSICS_GROUP_PLAYER_RIGHT_HAND |
                                                   ecs::PHYSICS_GROUP_USER_INTERFACE);
            } else if (handStr == "right") {
                handChar = 'r';
                inputScope.entity = "vr_actions_main_in_righthand_anim";
                controllerRef = ecs::Name("vr", "controller_right");
                actionPrefix = "actions_main_in_righthand_anim";
                collisionMask = (PhysicsGroupMask)(ecs::PHYSICS_GROUP_WORLD | ecs::PHYSICS_GROUP_WORLD_OVERLAP |
                                                   ecs::PHYSICS_GROUP_PLAYER_LEFT_HAND |
                                                   ecs::PHYSICS_GROUP_USER_INTERFACE);
            } else {
                Errorf("Invalid hand specified for VrHand script: %s", handStr);
                return false;
            }
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
    };

    static void handlePointing(ScriptData &scriptData,
        PhysicsUpdateLock lock,
        Entity ent,
        bool isPointing,
        double pointDistance) {

        glm::vec3 pointOrigin, pointDir, pointPos;
        Entity pointTarget;

        auto &query = ent.Get<PhysicsQuery>(lock);
        if (isPointing) {
            if (scriptData.physicsRefs[6] && scriptData.physicsRefs[7]) {
                auto indexBone0 = scriptData.physicsRefs[6].Get(lock);
                auto indexBone1 = scriptData.physicsRefs[7].Get(lock);

                if (indexBone0.Has<TransformSnapshot>(lock) && indexBone1.Has<TransformSnapshot>(lock)) {
                    auto &tr0 = indexBone0.Get<TransformSnapshot>(lock);
                    auto &tr1 = indexBone1.Get<TransformSnapshot>(lock);

                    pointOrigin = tr1.GetPosition();
                    pointDir = glm::normalize(pointOrigin - tr0.GetPosition());
                }
            }

            if (!scriptData.pointQuery) {
                scriptData.pointQuery = query.NewQuery(
                    PhysicsQuery::Raycast(pointDistance, PhysicsGroupMask(PHYSICS_GROUP_USER_INTERFACE)));
            }

            auto &pointQuery = query.Lookup(scriptData.pointQuery);

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

        if (scriptData.pointEntity && scriptData.pointEntity != pointTarget) {
            EventBindings::SendEvent(lock,
                scriptData.pointEntity,
                INTERACT_EVENT_INTERACT_POINT,
                Event{INTERACT_EVENT_INTERACT_POINT, ent, false});
        }
        if (pointTarget) {
            Transform pointTransform;
            pointTransform.SetPosition(pointPos);
            EventBindings::SendEvent(lock,
                pointTarget,
                INTERACT_EVENT_INTERACT_POINT,
                Event{INTERACT_EVENT_INTERACT_POINT, ent, pointTransform});
        }
        scriptData.pointEntity = pointTarget;

        auto laser = scriptData.laserPointerRef.Get(lock);
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

        Event event;
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
                if (std::get<bool>(event.data) && pointTarget) {
                    // Press the entity being looked at
                    EventBindings::SendEvent(lock,
                        pointTarget,
                        INTERACT_EVENT_INTERACT_PRESS,
                        Event{INTERACT_EVENT_INTERACT_PRESS, ent, true});
                    scriptData.pressEntity = pointTarget;
                }
            }
        }
    }

    InternalPhysicsScript vrHandScript("vr_hand",
        [](ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            ZoneScopedN("VrHandScript");
            if (!ent.Has<Name, Physics, PhysicsJoints, PhysicsQuery, TransformTree>(lock)) return;

            auto scene = state.scope.scene.lock();
            Assertf(scene, "VrHand script does not have a valid scene: %s", ToString(lock, ent));

            ScriptData scriptData = {};
            if (state.userData.has_value()) {
                scriptData = std::any_cast<ScriptData>(state.userData);
            } else {
                if (!scriptData.Init(state, lock, ent)) return;
            }

            auto &ph = ent.Get<Physics>(lock);
            auto &joints = ent.Get<PhysicsJoints>(lock).joints;
            auto &query = ent.Get<PhysicsQuery>(lock);
            auto &transform = ent.Get<TransformTree>(lock);
            auto inputRoot = scriptData.inputRootRef.Get(lock);
            auto controllerEnt = scriptData.controllerRef.Get(lock);

            // Read and update overlap queries
            EnumArray<Entity, BoneGroup> groupOverlaps = {};
            for (size_t i = 0; i < boneDefinitions.size(); i++) {
                if (!scriptData.inputRefs[i] || !scriptData.physicsRefs[i]) continue;

                auto inputEnt = scriptData.inputRefs[i].Get(lock);
                if (inputEnt.Has<TransformTree>(lock)) {
                    auto boneShape = scriptData.ShapeForBone(lock, inputRoot, i);
                    if (!scriptData.queries[i]) {
                        scriptData.queries[i] = query.NewQuery(
                            PhysicsQuery::Overlap{boneShape, scriptData.collisionMask});
                    } else {
                        auto &overlapQuery = query.Lookup(scriptData.queries[i]);
                        overlapQuery.shape = boneShape;
                        overlapQuery.filterGroup = scriptData.collisionMask;

                        if (overlapQuery.result && *overlapQuery.result) {
                            auto group = CVarHandOverlapTest.Get() == 2 ? BoneGroup::Wrist : boneDefinitions[i].group;
                            groupOverlaps[group] = *overlapQuery.result;
                        }
                    }

                    auto &inputTransform = inputEnt.Get<TransformTree>(lock);
                    scriptData.queryTransforms[i] = inputTransform.GetRelativeTransform(lock, inputRoot);
                }
            }

            bool forceTeleport = false;
            auto constraintTarget = scriptData.inputRootRef.Get(lock);
            if (constraintTarget.Has<TransformTree>(lock)) {
                auto &inputTransform = constraintTarget.Get<TransformTree>(lock);
                auto targetTransform = inputTransform.GetGlobalTransform(lock);
                // Don't set the hand constraint target until the controller is valid
                if (inputTransform.parent) {
                    if (joints.empty()) joints.resize(1);
                    if (joints[0].target != constraintTarget || joints[0].type != PhysicsJointType::Force) {
                        joints[0].target = scriptData.inputRootRef;
                        joints[0].type = PhysicsJointType::Force;
                        // TODO: Read this property from event or cvar
                        joints[0].limit = glm::vec2(20.0f, 10.0f);
                        forceTeleport = true;
                    }
                }
            }

            // Teleport the hands back to the player if they get too far away
            auto teleportDistance = state.GetParam<double>("teleport_distance");
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
            auto indexCurl = SignalBindings::GetSignal(lock, controllerEnt, scriptData.actionPrefix + "_curl_index");
            auto grabSignal = indexCurl;
            auto grabTarget = scriptData.grabEntity;
            if (teleported || grabSignal < 0.18) {
                grabTarget = {};
            } else if (grabSignal > 0.2 && !grabTarget) {
                grabTarget = groupOverlaps[BoneGroup::Index];
            }
            if (scriptData.grabEntity && scriptData.grabEntity != grabTarget) {
                // Drop the currently held entity
                EventBindings::SendEvent(lock,
                    scriptData.grabEntity,
                    INTERACT_EVENT_INTERACT_GRAB,
                    Event{INTERACT_EVENT_INTERACT_GRAB, ent, false});
                scriptData.grabEntity = {};
            }
            if (grabTarget && grabTarget != scriptData.grabEntity) {
                // Grab the entity being overlapped
                auto globalTransform = transform.GetGlobalTransform(lock);
                if (EventBindings::SendEvent(lock,
                        grabTarget,
                        INTERACT_EVENT_INTERACT_GRAB,
                        Event{INTERACT_EVENT_INTERACT_GRAB, ent, globalTransform}) > 0) {
                    scriptData.grabEntity = grabTarget;
                }
            }

            auto middleCurl = SignalBindings::GetSignal(lock, controllerEnt, scriptData.actionPrefix + "_curl_middle");
            bool isPointing = indexCurl < 0.05 && middleCurl > 0.5;
            handlePointing(scriptData, lock, ent, isPointing, state.GetParam<double>("point_distance"));

            // Update the hand's physics shape
            ph.shapes.clear();
            for (size_t i = 0; i < boneDefinitions.size(); i++) {
                if (!scriptData.inputRefs[i] || !scriptData.physicsRefs[i] || !scriptData.queries[i]) continue;

                auto physicsEnt = scriptData.physicsRefs[i].Get(lock);
                if (physicsEnt.Has<TransformTree>(lock)) {
                    auto group = CVarHandOverlapTest.Get() == 2 ? BoneGroup::Wrist : boneDefinitions[i].group;
                    if (!groupOverlaps[group] || CVarHandOverlapTest.Get() == 0) {
                        if (!scriptData.grabEntity) {
                            // This group doesn't overlap, so update the current pose and shape
                            auto &overlapQuery = query.Lookup(scriptData.queries[i]);
                            scriptData.currentShapes[i] = overlapQuery.shape;

                            auto &physicsTransform = physicsEnt.Get<TransformTree>(lock);
                            physicsTransform.parent = scriptData.physicsRootRef;
                            physicsTransform.pose = scriptData.queryTransforms[i];
                        }
                    }
                    if (scriptData.currentShapes[i]) ph.shapes.push_back(scriptData.currentShapes[i]);
                }
            }

            state.userData = scriptData;
        });
} // namespace sp::scripts
