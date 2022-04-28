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

            std::array<std::string, 5> fingerNames = {"thumb", "index", "middle", "ring", "pinky"};
            std::array<std::string, 5> fingerSegmentNames = {
                "meta_" + handStr,
                "0_" + handStr,
                "1_" + handStr,
                "2_" + handStr,
                handStr + "_end",
            };

            for (auto &fingerName : fingerNames) {
                for (auto &segmentName : fingerSegmentNames) {
                    std::string boneName = "finger_" + fingerName + "_" + segmentName;
                    ecs::Name inputName(boneName, inputScope);

                    EntityRef inputEntity(inputName);
                    if (!inputEntity) {
                        Errorf("VrHand script has invalid input entity: %s", inputName.String());
                        continue;
                    }

                    // Physics physics;
                    // physics.shapes.emplace_back(PhysicsShape::Capsule(0.5f, 0.01f));

                    // physics.constraint = inputEntity;
                    // physics.group = PhysicsGroup::Player;
                    // Component<Physics>::Apply(physics, lock, physicsEnt);
                }
            }

            {
                ecs::Name inputName("wrist_" + handStr, inputScope);

                EntityRef inputEntity(inputName);
                if (!inputEntity) {
                    Errorf("VrHand script has invalid input entity: %s", inputName.String());
                    return;
                }

                // Physics physics;
                // auto &boxShape = physics.shapes.emplace_back(PhysicsShape::Box(glm::vec3(0.04, 0.095, 0.11)));
                // boxShape.transform.Translate(glm::vec3(0.005, 0.01, 0.03));

                // physics.constraint = inputEntity;
                // physics.group = PhysicsGroup::PlayerHands;
                // Component<Physics>::Apply(physics, lock, physicsEnt);
            }
        }),
    };
} // namespace ecs
