#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace ecs {
    std::array physicsScripts = {
        InternalPhysicsScript("voxel_controller",
            [](ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<TransformTree, VoxelArea>(lock)) {
                    auto &transform = ent.Get<TransformTree>(lock);
                    auto &voxelArea = ent.Get<VoxelArea>(lock);
                    auto voxelRotation = transform.GetGlobalRotation(lock);

                    auto voxelScale = (float)state.GetParam<double>("voxel_scale");
                    auto voxelStride = std::max(1.0f, (float)state.GetParam<double>("voxel_stride"));
                    glm::vec3 voxelOffset;
                    voxelOffset.x = state.GetParam<double>("voxel_offset_x");
                    voxelOffset.y = state.GetParam<double>("voxel_offset_y");
                    voxelOffset.z = state.GetParam<double>("voxel_offset_z");
                    voxelOffset *= glm::vec3(voxelArea.extents) * voxelScale;

                    ecs::Name alignmentName(state.GetParam<std::string>("alignment_target"), state.scope.prefix);
                    if (alignmentName) {
                        auto alignmentEntity = state.GetParam<EntityRef>("alignment_entity");
                        bool existingAlignment = (bool)alignmentEntity;
                        if (alignmentEntity.Name() != alignmentName) alignmentEntity = alignmentName;

                        auto previousAlignment = state.GetParam<glm::vec3>("previous_alignment");
                        glm::vec3 alignmentOffset;
                        auto target = alignmentEntity.Get(lock);
                        if (target) {
                            state.SetParam<EntityRef>("alignment_entity", alignmentEntity);

                            if (target.Has<TransformSnapshot>(lock)) {
                                if (existingAlignment) {
                                    alignmentOffset = target.Get<TransformSnapshot>(lock).GetPosition() -
                                                      previousAlignment;
                                } else {
                                    state.SetParam<glm::vec3>("previous_alignment",
                                        target.Get<TransformSnapshot>(lock).GetPosition());
                                }
                            } else {
                                state.SetParam<EntityRef>("alignment_entity", EntityRef());
                            }
                        } else {
                            state.SetParam<EntityRef>("alignment_entity", EntityRef());
                        }
                        voxelOffset += glm::mod(alignmentOffset, voxelStride * voxelScale);
                    }

                    auto targetPosition = glm::vec3(0);
                    ecs::Name targetName(state.GetParam<std::string>("follow_target"), state.scope.prefix);
                    if (targetName) {
                        auto followEntity = state.GetParam<EntityRef>("follow_entity");
                        if (followEntity.Name() != targetName) followEntity = targetName;

                        auto target = followEntity.Get(lock);
                        if (target) {
                            state.SetParam<EntityRef>("follow_entity", followEntity);

                            if (target.Has<TransformSnapshot>(lock)) {
                                targetPosition = target.Get<TransformSnapshot>(lock).GetPosition();
                            }
                        }
                    }

                    targetPosition = voxelRotation * targetPosition;
                    targetPosition = glm::floor(targetPosition / voxelStride / voxelScale) * voxelScale * voxelStride;
                    transform.pose.SetPosition(glm::inverse(voxelRotation) * (targetPosition + voxelOffset));
                    transform.pose.SetScale(glm::vec3(voxelScale));
                }
            }),
        InternalPhysicsScript("vr_hand",
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

                    struct SegmentProperties {
                        std::string name;
                        float radius;
                    };
                    static const std::array fingerSegments = {
                        SegmentProperties{"finger_thumb_0_#", 0.015f},
                        SegmentProperties{"finger_thumb_1_#", 0.01f},
                        SegmentProperties{"finger_thumb_2_#", 0.01f},
                        SegmentProperties{"finger_thumb_#_end", 0.008f},

                        SegmentProperties{"finger_index_meta_#", 0.015f},
                        SegmentProperties{"finger_index_0_#", 0.015f},
                        SegmentProperties{"finger_index_1_#", 0.01f},
                        SegmentProperties{"finger_index_2_#", 0.01f},
                        SegmentProperties{"finger_index_#_end", 0.008f},

                        SegmentProperties{"finger_middle_meta_#", 0.015f},
                        SegmentProperties{"finger_middle_0_#", 0.015f},
                        SegmentProperties{"finger_middle_1_#", 0.01f},
                        SegmentProperties{"finger_middle_2_#", 0.01f},
                        SegmentProperties{"finger_middle_#_end", 0.008f},

                        SegmentProperties{"finger_ring_meta_#", 0.015f},
                        SegmentProperties{"finger_ring_0_#", 0.015f},
                        SegmentProperties{"finger_ring_1_#", 0.01f},
                        SegmentProperties{"finger_ring_2_#", 0.01f},
                        SegmentProperties{"finger_ring_#_end", 0.008f},

                        SegmentProperties{"finger_pinky_meta_#", 0.015f},
                        SegmentProperties{"finger_pinky_0_#", 0.015f},
                        SegmentProperties{"finger_pinky_1_#", 0.01f},
                        SegmentProperties{"finger_pinky_2_#", 0.01f},
                        SegmentProperties{"finger_pinky_#_end", 0.008f},
                    };

                    auto inputRoot = EntityRef(inputScope).Get(lock);
                    if (!inputRoot) {
                        Errorf("VrHand script has invalid input root: %s", inputScope.String());
                        return;
                    }

                    auto shapeForBone = [&](const TransformTree &bone, const SegmentProperties &segment) {
                        auto relativeTransform = bone.GetRelativeTransform(lock, inputRoot);
                        PhysicsShape shape;
                        shape.transform = relativeTransform;

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

                        shape.shape = PhysicsShape::Capsule(boneLength, segment.radius);
                        // shape.shape = PhysicsShape::Box(glm::vec3(boneLength, segment.radius, segment.radius));

                        auto boneDiff = parentTransform.GetPosition() - relativeTransform.GetPosition();
                        glm::vec3 boneVector = glm::vec4(boneDiff, 0.0f);
                        // Place the center of the capsule halfway between this bone and its parent
                        shape.transform.Translate(boneVector * 0.5f);
                        // Rotate the capsule so it's aligned with the bone vector
                        shape.transform.SetRotation(glm::quat(glm::vec3(1, 0, 0), boneVector));
                        return shape;
                    };

                    auto scene = state.scope.scene.lock();
                    Assertf(scene, "VrHand script does not have a valid scene: %s", ToString(lock, ent));

                    auto &prefabName = ent.Get<Name>(lock);
                    ecs::Name physicsScope(state.scope.prefix.scene, prefabName.entity);

                    auto physicsRoot = EntityRef(physicsScope).Get(lock);
                    if (!physicsRoot) {
                        Errorf("VrHand script has invalid physics root: %s", physicsScope.String());
                        return;
                    }

                    auto &ph = ent.Get<Physics>(lock);
                    auto &query = ent.Get<PhysicsQuery>(lock);

                    ph.shapes.clear();
                    query.queries.clear();
                    std::string boneName;
                    ecs::Name inputName;
                    ecs::Name physicsName;
                    for (auto &segment : fingerSegments) {
                        boneName = segment.name;
                        std::transform(boneName.begin(), boneName.end(), boneName.begin(), [&](unsigned char c) {
                            return c == '#' ? handChar : c;
                        });
                        inputName.Parse(boneName, inputScope);
                        physicsName.Parse(boneName, physicsScope);

                        EntityRef inputEntity(inputName);
                        if (!inputEntity) {
                            Errorf("VrHand script has invalid input entity: %s", inputName.String());
                            continue;
                        }

                        EntityRef physicsEntity(physicsName);
                        if (!physicsEntity) {
                            Errorf("VrHand script has invalid physics entity: %s", physicsName.String());
                            continue;
                        }

                        auto inputEnt = inputEntity.Get(lock);
                        auto physicsEnt = physicsEntity.Get(lock);
                        if (inputEnt.Has<TransformTree>(lock) && physicsEnt.Has<TransformTree>(lock)) {
                            auto &boneTransform = inputEnt.Get<const TransformTree>(lock);
                            ph.shapes.push_back(shapeForBone(boneTransform, segment));

                            auto &physicsTransform = physicsEnt.Get<TransformTree>(lock);
                            physicsTransform.pose = boneTransform.GetRelativeTransform(lock, inputRoot);
                            physicsTransform.parent = physicsRoot;
                        }
                    }

                    {
                        boneName = "wrist_#";
                        std::transform(boneName.begin(), boneName.end(), boneName.begin(), [&](unsigned char c) {
                            return c == '#' ? handChar : c;
                        });
                        inputName.Parse(boneName, inputScope);
                        physicsName.Parse(boneName, physicsScope);

                        EntityRef inputEntity(inputName);
                        if (!inputEntity) {
                            Errorf("VrHand script has invalid input entity: %s", inputName.String());
                            return;
                        }

                        EntityRef physicsEntity(physicsName);
                        if (!physicsEntity) {
                            Errorf("VrHand script has invalid physics entity: %s", physicsName.String());
                            return;
                        }

                        auto inputEnt = inputEntity.Get(lock);
                        auto physicsEnt = physicsEntity.Get(lock);
                        if (inputEnt.Has<TransformSnapshot>(lock) && physicsEnt.Has<TransformTree>(lock)) {
                            auto relTransform = inputEnt.Get<const TransformTree>(lock).GetRelativeTransform(lock,
                                inputRoot);
                            auto &shape = ph.shapes.emplace_back(PhysicsShape::Box(glm::vec3(0.04, 0.07, 0.06)));
                            shape.transform = relTransform;
                            shape.transform.Translate(shape.transform.GetRotation() * glm::vec3(0.01, 0.0, 0.01));

                            auto &physicsTransform = physicsEnt.Get<TransformTree>(lock);
                            physicsTransform.pose = relTransform;
                            physicsTransform.parent = physicsRoot;
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
                }
            }),
    };
} // namespace ecs
