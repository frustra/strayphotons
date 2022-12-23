#include "assets/AssetManager.hh"
#include "assets/Gltf.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"

namespace sp::scripts {
    using namespace ecs;

    struct VoxelController {
        float voxelScale = 0.1f;
        float voxelStride = 1.0f;
        glm::vec3 voxelOffset;
        EntityRef alignmentEntity, followEntity;
        std::optional<glm::vec3> alignment;

        void OnPhysicsUpdate(ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
            if (!ent.Has<TransformTree, VoxelArea>(lock) || voxelScale == 0.0f || voxelStride < 1.0f) return;

            auto &transform = ent.Get<TransformTree>(lock);
            auto &voxelArea = ent.Get<VoxelArea>(lock);
            auto voxelRotation = transform.GetGlobalRotation(lock);

            glm::vec3 offset = voxelOffset * glm::vec3(voxelArea.extents) * voxelScale;
            auto alignmentTarget = alignmentEntity.Get(lock);
            if (alignmentTarget.Has<TransformSnapshot>(lock)) {
                glm::vec3 alignmentOffset;
                if (alignment) {
                    alignmentOffset = alignmentTarget.Get<TransformSnapshot>(lock).GetPosition() - alignment.value();
                } else {
                    alignment = alignmentTarget.Get<TransformSnapshot>(lock).GetPosition();
                }
                offset += glm::mod(alignmentOffset, voxelStride * voxelScale);
            }

            auto followPosition = glm::vec3(0);
            auto followTarget = followEntity.Get(lock);
            if (followTarget.Has<TransformSnapshot>(lock)) {
                followPosition = followTarget.Get<TransformSnapshot>(lock).GetPosition();
            }

            followPosition = voxelRotation * followPosition;
            followPosition = glm::floor(followPosition / voxelStride / voxelScale) * voxelScale * voxelStride;
            transform.pose.SetPosition(glm::inverse(voxelRotation) * (followPosition + offset));
            transform.pose.SetScale(glm::vec3(voxelScale));
        }
    };
    StructMetadata MetadataVoxelController(typeid(VoxelController),
        StructField::New("voxel_scale", &VoxelController::voxelScale),
        StructField::New("voxel_stride", &VoxelController::voxelStride),
        StructField::New("voxel_offset", &VoxelController::voxelOffset),
        StructField::New("alignment_target", &VoxelController::alignmentEntity),
        StructField::New("follow_target", &VoxelController::followEntity));
    InternalPhysicsScript2<VoxelController> voxelController("voxel_controller", MetadataVoxelController);

    // std::array physicsScripts = {
    //     InternalPhysicsScript("rotate",
    //         [](ScriptState &state, PhysicsUpdateLock lock, Entity ent, chrono_clock::duration interval) {
    //             if (ent.Has<TransformTree>(lock)) {
    //                 glm::vec3 rotationAxis;
    //                 rotationAxis.x = state.GetParam<double>("axis_x");
    //                 rotationAxis.y = state.GetParam<double>("axis_y");
    //                 rotationAxis.z = state.GetParam<double>("axis_z");
    //                 auto rotationSpeedRpm = state.GetParam<double>("speed");

    //                 auto &transform = ent.Get<TransformTree>(lock);
    //                 auto currentRotation = transform.pose.GetRotation();
    //                 transform.pose.SetRotation(glm::rotate(currentRotation,
    //                     (float)(rotationSpeedRpm * M_PI * 2.0 / 60.0 * interval.count() / 1e9),
    //                     rotationAxis));
    //             }
    //         }),
    // };
} // namespace sp::scripts
