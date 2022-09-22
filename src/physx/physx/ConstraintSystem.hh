#pragma once

#include "ecs/Ecs.hh"

namespace physx {
    class PxRigidActor;
}

namespace sp {
    class PhysxManager;
    struct JointState;

    class ConstraintSystem {
    public:
        ConstraintSystem(PhysxManager &manager);
        ~ConstraintSystem() {}

        void Frame(ecs::Lock<
            ecs::Read<ecs::TransformTree, ecs::CharacterController, ecs::Physics, ecs::PhysicsJoints, ecs::SceneInfo>>
                lock);

        void BreakConstraints(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::CharacterController>,
            ecs::Write<ecs::Physics>,
            ecs::SendEventsLock> lock);

    private:
        void UpdateForceConstraint(physx::PxRigidActor *actor,
            JointState *joint,
            ecs::Transform transform,
            ecs::Transform targetTransform,
            glm::vec3 targetVelocity,
            glm::vec3 gravity);

        void HandleForceLimitConstraint(physx::PxRigidActor *actor,
            const ecs::Physics &physics,
            ecs::Transform transform,
            ecs::Transform targetTransform,
            glm::vec3 targetVelocity);

        void UpdateJoints(ecs::Lock<ecs::Read<ecs::TransformTree, ecs::PhysicsJoints, ecs::SceneInfo>> lock,
            ecs::Entity entity,
            physx::PxRigidActor *actor,
            ecs::Transform transform);

        void ReleaseJoints(ecs::Entity entity, physx::PxRigidActor *actor);

        PhysxManager &manager;
    };
} // namespace sp
