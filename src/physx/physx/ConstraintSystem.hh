#pragma once

#include "ecs/Ecs.hh"

namespace physx {
    class PxRigidActor;
}

namespace sp {
    class PhysxManager;

    class ConstraintSystem {
    public:
        ConstraintSystem(PhysxManager &manager);
        ~ConstraintSystem() {}

        void Frame(
            ecs::Lock<ecs::Read<ecs::TransformTree, ecs::CharacterController, ecs::Physics, ecs::PhysicsJoints>> lock);

        void BreakConstraints(
            ecs::Lock<ecs::Read<ecs::TransformSnapshot>, ecs::Write<ecs::Physics>, ecs::SendEventsLock> lock);

    private:
        void HandleForceLimitConstraint(physx::PxRigidActor *actor,
            const ecs::Physics &physics,
            ecs::Transform transform,
            ecs::Transform targetTransform,
            glm::vec3 targetVelocity);

        void UpdateJoints(ecs::Lock<ecs::Read<ecs::TransformTree, ecs::PhysicsJoints>> lock,
            ecs::Entity entity,
            physx::PxRigidActor *actor,
            ecs::Transform transform);

        void ReleaseJoints(ecs::Entity entity, physx::PxRigidActor *actor);

        PhysxManager &manager;
    };
} // namespace sp
