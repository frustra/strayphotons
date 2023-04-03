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

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            if (!entLock.Has<TransformTree, VoxelArea>() || voxelScale == 0.0f || voxelStride < 1.0f) return;

            auto &transform = entLock.Get<TransformTree>();
            auto &voxelArea = entLock.Get<VoxelArea>();
            auto voxelRotation = transform.GetGlobalRotation(entLock);

            glm::vec3 offset = voxelOffset * glm::vec3(voxelArea.extents) * voxelScale;
            auto alignmentTarget = alignmentEntity.Get(entLock);
            if (alignmentTarget.Has<TransformSnapshot>(entLock)) {
                glm::vec3 alignmentOffset;
                if (alignment) {
                    alignmentOffset = alignmentTarget.Get<TransformSnapshot>(entLock).GetPosition() - alignment.value();
                } else {
                    alignment = alignmentTarget.Get<TransformSnapshot>(entLock).GetPosition();
                }
                offset += glm::mod(alignmentOffset, voxelStride * voxelScale);
            }

            auto followPosition = glm::vec3(0);
            auto followTarget = followEntity.Get(entLock);
            if (followTarget.Has<TransformSnapshot>(entLock)) {
                followPosition = followTarget.Get<TransformSnapshot>(entLock).GetPosition();
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
    InternalPhysicsScript<VoxelController> voxelController("voxel_controller", MetadataVoxelController);

    struct RotatePhysics {
        glm::vec3 rotationAxis;
        float rotationSpeedRpm;

        void OnPhysicsUpdate(ScriptState &state,
            EntityLock<PhysicsUpdateLock> entLock,
            chrono_clock::duration interval) {
            if (!entLock.Has<TransformTree>() || rotationAxis == glm::vec3(0) || rotationSpeedRpm == 0.0f) return;

            auto &transform = entLock.Get<TransformTree>();
            auto currentRotation = transform.pose.GetRotation();
            transform.pose.SetRotation(glm::rotate(currentRotation,
                (float)(rotationSpeedRpm * M_PI * 2.0 / 60.0 * interval.count() / 1e9),
                rotationAxis));
        }
    };
    StructMetadata MetadataRotatePhysics(typeid(RotatePhysics),
        StructField::New("axis", &RotatePhysics::rotationAxis),
        StructField::New("speed", &RotatePhysics::rotationSpeedRpm));
    InternalPhysicsScript<RotatePhysics> rotatePhysics("rotate_physics", MetadataRotatePhysics);
} // namespace sp::scripts
