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
        InternalPhysicsScript("rotate",
            [](ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
                if (ent.Has<TransformTree>(lock)) {
                    glm::vec3 rotationAxis;
                    rotationAxis.x = state.GetParam<double>("axis_x");
                    rotationAxis.y = state.GetParam<double>("axis_y");
                    rotationAxis.z = state.GetParam<double>("axis_z");
                    auto rotationSpeedRpm = state.GetParam<double>("speed");

                    auto &transform = ent.Get<TransformTree>(lock);
                    auto currentRotation = transform.pose.GetRotation();
                    transform.pose.SetRotation(glm::rotate(currentRotation,
                        (float)(rotationSpeedRpm * M_PI * 2.0 / 60.0 * interval.count() / 1e9),
                        rotationAxis));
                }
            }),
    };
} // namespace ecs
