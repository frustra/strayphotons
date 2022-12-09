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
                if (!ent.Has<TransformTree, VoxelArea>(lock)) return;

                struct ScriptData {
                    EntityRef alignmentEntity, followEntity;
                    std::optional<glm::vec3> alignment;
                };

                ScriptData scriptData;
                if (state.userData.has_value()) {
                    scriptData = std::any_cast<ScriptData>(state.userData);
                } else {
                    auto alignmentTargetName = state.GetParam<std::string>("alignment_target");
                    auto followTargetName = state.GetParam<std::string>("follow_target");
                    ecs::Name alignmentName(alignmentTargetName, state.scope.prefix);
                    ecs::Name followName(followTargetName, state.scope.prefix);
                    if (!alignmentName) {
                        Errorf("Voxel controller alignment target name is invalid: %s", alignmentTargetName);
                        return;
                    }
                    if (!followName) {
                        Errorf("Voxel controller follow target name is invalid: %s", followTargetName);
                        return;
                    }
                    scriptData.alignmentEntity = alignmentName;
                    scriptData.followEntity = followName;
                }

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

                auto alignmentTarget = scriptData.alignmentEntity.Get(lock);
                if (alignmentTarget.Has<TransformSnapshot>(lock)) {
                    glm::vec3 alignmentOffset;
                    if (scriptData.alignment) {
                        alignmentOffset = alignmentTarget.Get<TransformSnapshot>(lock).GetPosition() -
                                          scriptData.alignment.value();
                    } else {
                        scriptData.alignment = alignmentTarget.Get<TransformSnapshot>(lock).GetPosition();
                    }
                    voxelOffset += glm::mod(alignmentOffset, voxelStride * voxelScale);
                }

                auto followPosition = glm::vec3(0);
                auto followTarget = scriptData.followEntity.Get(lock);
                if (followTarget.Has<TransformSnapshot>(lock)) {
                    followPosition = followTarget.Get<TransformSnapshot>(lock).GetPosition();
                }

                followPosition = voxelRotation * followPosition;
                followPosition = glm::floor(followPosition / voxelStride / voxelScale) * voxelScale * voxelStride;
                transform.pose.SetPosition(glm::inverse(voxelRotation) * (followPosition + voxelOffset));
                transform.pose.SetScale(glm::vec3(voxelScale));

                state.userData = scriptData;
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
