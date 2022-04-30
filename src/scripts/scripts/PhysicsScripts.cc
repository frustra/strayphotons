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
                if (ent.Has<Name, Physics, PhysicsQuery>(lock)) {
                    auto handStr = state.GetParam<std::string>("hand");
                    sp::to_lower(handStr);
                    ecs::Name inputScope("input", "");
                    if (handStr == "left") {
                        handStr = "l";
                        inputScope.entity = "vr_actions_main_in_lefthand_anim";
                    } else if (handStr == "right") {
                        handStr = "r";
                        inputScope.entity = "vr_actions_main_in_righthand_anim";
                    } else {
                        Abortf("Invalid hand specified for VrHand script: %s", handStr);
                    }

                    auto scene = state.scope.scene.lock();
                    Assertf(scene, "VrHand script does not have a valid scene: %s", ToString(lock, ent));

                    auto &prefabName = ent.Get<Name>(lock);
                    ecs::Name physicsScope(state.scope.prefix.scene, prefabName.entity);

                    auto &ph = ent.Get<Physics>(lock);
                    // auto &query = ent.Get<PhysicsQuery>(lock);

                    std::array<std::string, 5> fingerNames = {"thumb", "index", "middle", "ring", "pinky"};
                    struct SegmentProperties {
                        std::string name;
                        float radius;
                        glm::vec3 offset;
                    };
                    std::array fingerSegments = {
                        SegmentProperties{"meta_" + handStr, 0.015f, glm::vec3(0)},
                        SegmentProperties{"0_" + handStr, 0.015f, glm::vec3(0)},
                        SegmentProperties{"1_" + handStr, 0.01f, glm::vec3(0)},
                        SegmentProperties{"2_" + handStr, 0.01f, glm::vec3(0)},
                        SegmentProperties{handStr + "_end", 0.008f, glm::vec3(0)},
                    };

                    auto &rootTransform = ent.Get<TransformSnapshot>(lock);
                    auto invRootTransform = rootTransform.GetInverse();

                    auto scale = rootTransform.GetScale();
                    auto avgScale = (scale.x + scale.y + scale.z) / 3;

                    auto shapeForBone = [&](const TransformTree &bone, const SegmentProperties &segment) {
                        auto globalTransform = bone.GetGlobalTransform(lock);
                        PhysicsShape shape;
                        shape.transform = invRootTransform * globalTransform;

                        auto parentEntity = bone.parent.Get(lock);
                        if (!parentEntity.Has<TransformTree>(lock)) {
                            shape.shape = PhysicsShape::Sphere(avgScale * segment.radius);
                            return shape;
                        }
                        auto parentTransform = parentEntity.Get<const TransformTree>(lock).GetGlobalTransform(lock);

                        float boneLength = glm::length(bone.pose.GetPosition());
                        if (boneLength <= 0.000001f) {
                            shape.shape = PhysicsShape::Sphere(avgScale * segment.radius);
                            return shape;
                        }

                        // shape.shape = PhysicsShape::Capsule(avgScale * boneLength, avgScale * segment.radius);
                        shape.shape = PhysicsShape::Box(
                            avgScale * glm::vec3(boneLength, segment.radius, segment.radius));

                        auto boneDiff = parentTransform.GetPosition() - globalTransform.GetPosition();
                        glm::vec3 boneVector = invRootTransform * glm::vec4(boneDiff, 0.0f);
                        // Place the center of the capsule halfway between this bone and its parent
                        shape.transform.Translate(boneVector * 0.5f);
                        // Rotate the capsule so it's aligned with the bone vector
                        shape.transform.SetRotation(glm::quat(glm::vec3(1, 0, 0), boneVector));
                        shape.transform.SetPosition(shape.transform.GetPosition() * scale);
                        return shape;
                    };

                    ph.shapes.clear();
                    for (auto &fingerName : fingerNames) {
                        for (auto &segment : fingerSegments) {
                            std::string boneName = "finger_" + fingerName + "_" + segment.name;
                            ecs::Name inputName(boneName, inputScope);
                            ecs::Name physicsName(boneName, physicsScope);

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
                            if (inputEnt.Has<TransformTree>(lock)) {
                                auto &boneTransform = inputEnt.Get<const TransformTree>(lock);
                                ph.shapes.push_back(shapeForBone(boneTransform, segment));
                            }

                            auto physicsEnt = physicsEntity.Get(lock);
                            if (physicsEnt.Has<TransformTree>(lock)) {
                                auto &boneTransform = physicsEnt.Get<TransformTree>(lock);
                                boneTransform.pose = {};
                                boneTransform.parent = inputEntity;
                            }
                        }
                    }

                    {
                        ecs::Name inputName("wrist_" + handStr, inputScope);
                        ecs::Name physicsName("wrist_" + handStr, physicsScope);

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
                        if (inputEnt.Has<TransformSnapshot>(lock)) {
                            auto &phTransform = inputEnt.Get<TransformSnapshot>(lock);
                            auto &shape = ph.shapes.emplace_back(
                                PhysicsShape::Box(avgScale * glm::vec3(0.04, 0.07, 0.06)));
                            shape.transform = invRootTransform * phTransform;
                            shape.transform.Translate(shape.transform.GetRotation() * glm::vec3(0.01, 0.0, 0.01));
                            shape.transform.SetPosition(shape.transform.GetPosition() * avgScale);
                        }

                        auto physicsEnt = physicsEntity.Get(lock);
                        if (physicsEnt.Has<TransformTree>(lock)) {
                            auto &boneTransform = physicsEnt.Get<TransformTree>(lock);
                            boneTransform.pose = {};
                            boneTransform.parent = inputEntity;
                        }
                    }
                }
            }),
    };
} // namespace ecs
