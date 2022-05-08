#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "core/Common.hh"
#include "core/EnumArray.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {

    static sp::CVar<int> CVarHandCollisionShapes("p.HandCollisionShapes", 1, "0: boxes, 1: capsules");
    static sp::CVar<bool> CVarHandOverlapTest("p.HandOverlapTest",
        true,
        "Only move each finger if nothing is in the way");

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

    InternalPhysicsScript vrHandScript("vr_hand",
        [](ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            ZoneScopedN("VrHandScript");
            if (ent.Has<Name, Physics, PhysicsQuery>(lock)) {
                auto handStr = state.GetParam<std::string>("hand");
                sp::to_lower(handStr);

                char handChar = 'l';
                ecs::Name inputScope("input", "");
                if (handStr == "left") {
                    handChar = 'l';
                    inputScope.entity = "vr_actions_main_in_lefthand_anim";
                } else if (handStr == "right") {
                    handChar = 'r';
                    inputScope.entity = "vr_actions_main_in_righthand_anim";
                } else {
                    Abortf("Invalid hand specified for VrHand script: %s", handStr);
                }

                auto inputRoot = EntityRef(inputScope).Get(lock);
                if (!inputRoot) {
                    Errorf("VrHand script has invalid input root: %s", inputScope.String());
                    return;
                }

                auto scene = state.scope.scene.lock();
                Assertf(scene, "VrHand script does not have a valid scene: %s", ToString(lock, ent));

                auto &prefabName = ent.Get<Name>(lock);
                ecs::Name physicsScope(state.scope.prefix.scene, prefabName.entity);

                struct ScriptData {
                    std::array<EntityRef, boneDefinitions.size()> inputRefs;
                    std::array<EntityRef, boneDefinitions.size()> physicsRefs;
                    std::array<PhysicsQuery::Handle<PhysicsQuery::Overlap>, boneDefinitions.size()> queries;
                    std::array<Transform, boneDefinitions.size()> queryTransforms;
                    std::array<PhysicsShape, boneDefinitions.size()> currentShapes;
                    PhysicsGroupMask collisionMask;
                };

                ScriptData scriptData = {};
                if (state.userData.has_value()) {
                    scriptData = std::any_cast<ScriptData>(state.userData);
                } else {
                    for (size_t i = 0; i < boneDefinitions.size(); i++) {
                        std::string name(boneDefinitions[i].boneName);
                        std::transform(name.begin(), name.end(), name.begin(), [&](unsigned char c) {
                            return c == '#' ? handChar : c;
                        });

                        scriptData.inputRefs[i] = ecs::Name(name, inputScope);
                        scriptData.physicsRefs[i] = ecs::Name(name, physicsScope);

                        if (!scriptData.inputRefs[i]) {
                            Errorf("VrHand has invalid input entity: %s with scope %s", name, inputScope.String());
                        }
                        if (!scriptData.physicsRefs[i]) {
                            Errorf("VrHand has invalid physics entity: %s with scope %s", name, physicsScope.String());
                        }
                    }

                    if (handChar == 'l') {
                        scriptData.collisionMask = (PhysicsGroupMask)(ecs::PHYSICS_GROUP_WORLD |
                                                                      ecs::PHYSICS_GROUP_HELD_OBJECT |
                                                                      ecs::PHYSICS_GROUP_PLAYER_RIGHT_HAND);
                    } else {
                        scriptData.collisionMask = (PhysicsGroupMask)(ecs::PHYSICS_GROUP_WORLD |
                                                                      ecs::PHYSICS_GROUP_HELD_OBJECT |
                                                                      ecs::PHYSICS_GROUP_PLAYER_LEFT_HAND);
                    }
                }

                auto shapeForBone = [&](size_t index) {
                    auto inputEnt = scriptData.inputRefs[index].Get(lock);
                    if (!inputEnt.Has<TransformTree>(lock)) return PhysicsShape();

                    auto &segment = boneDefinitions[index];
                    auto &bone = inputEnt.Get<const TransformTree>(lock);

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
                    auto parentTransform = parentEntity.Get<const TransformTree>(lock).GetRelativeTransform(lock,
                        inputRoot);

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

                auto physicsRoot = EntityRef(physicsScope).Get(lock);
                if (!physicsRoot) {
                    Errorf("VrHand script has invalid physics root: %s", physicsScope.String());
                    return;
                }

                auto &ph = ent.Get<Physics>(lock);
                auto &query = ent.Get<PhysicsQuery>(lock);

                sp::EnumArray<bool, BoneGroup> groupOverlaps = {};
                for (size_t i = 0; i < boneDefinitions.size(); i++) {
                    if (!scriptData.inputRefs[i] || !scriptData.physicsRefs[i]) continue;

                    auto inputEnt = scriptData.inputRefs[i].Get(lock);
                    if (inputEnt.Has<TransformTree>(lock)) {
                        auto boneShape = shapeForBone(i);
                        if (!scriptData.queries[i]) {
                            scriptData.queries[i] = query.NewQuery(
                                PhysicsQuery::Overlap{boneShape, scriptData.collisionMask});
                        } else {
                            auto &overlapQuery = query.Lookup(scriptData.queries[i]);
                            overlapQuery.shape = boneShape;
                            overlapQuery.filterGroup = scriptData.collisionMask;

                            if (overlapQuery.result && *overlapQuery.result) {
                                groupOverlaps[boneDefinitions[i].group] = true;
                            }
                        }

                        auto &inputTransform = inputEnt.Get<TransformTree>(lock);
                        scriptData.queryTransforms[i] = inputTransform.GetRelativeTransform(lock, inputRoot);
                    }
                }

                ph.shapes.clear();
                for (size_t i = 0; i < boneDefinitions.size(); i++) {
                    if (!scriptData.inputRefs[i] || !scriptData.physicsRefs[i] || !scriptData.queries[i]) continue;

                    auto physicsEnt = scriptData.physicsRefs[i].Get(lock);
                    if (physicsEnt.Has<TransformTree>(lock)) {
                        if (!groupOverlaps[boneDefinitions[i].group] || !CVarHandOverlapTest.Get()) {
                            // This group doesn't overlap, so update the current pose and shape
                            auto &overlapQuery = query.Lookup(scriptData.queries[i]);
                            scriptData.currentShapes[i] = overlapQuery.shape;

                            auto &physicsTransform = physicsEnt.Get<TransformTree>(lock);
                            physicsTransform.parent = physicsRoot;
                            physicsTransform.pose = scriptData.queryTransforms[i];
                        }
                        if (scriptData.currentShapes[i]) ph.shapes.push_back(scriptData.currentShapes[i]);
                    }
                }

                bool forceTeleport = false;
                auto constraintTarget = EntityRef(inputScope).Get(lock);
                if (constraintTarget.Has<TransformTree>(lock)) {
                    auto &transform = constraintTarget.Get<TransformTree>(lock);
                    if (transform.parent) {
                        // Don't set the hand constraint target until the controller is valid
                        if (ph.constraint != constraintTarget) {
                            ph.constraint = inputScope;
                            forceTeleport = true;
                        }
                    }
                }

                // Teleport the hands back to the player if they get too far away
                if (ent.Has<TransformTree>(lock)) {
                    auto teleportDistance = state.GetParam<double>("teleport_distance");
                    if (teleportDistance > 0 || forceTeleport) {
                        // TODO: Release any objects the player is holding
                        auto parentEnt = ph.constraint.Get(lock);
                        if (parentEnt.Has<TransformTree>(lock)) {
                            auto &transform = ent.Get<TransformTree>(lock);
                            Assertf(!transform.parent, "vr_hand script transform can't have parent");
                            auto parentTransform = parentEnt.Get<TransformTree>(lock).GetGlobalTransform(lock);

                            auto dist = glm::length(transform.pose.GetPosition() - parentTransform.GetPosition());
                            if (dist >= teleportDistance || forceTeleport) transform.pose = parentTransform;
                        }
                    }
                }

                state.userData = scriptData;
            }
        });
} // namespace ecs
