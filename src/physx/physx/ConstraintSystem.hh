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

        void Frame(
            ecs::Lock<ecs::Read<ecs::TransformTree, ecs::CharacterController, ecs::Physics, ecs::SceneProperties>,
                ecs::Write<ecs::PhysicsJoints>> lock);

    private:
        bool UpdateForceConstraint(physx::PxRigidActor *actor,
            JointState *joint,
            ecs::Transform transform,
            ecs::Transform targetTransform,
            glm::vec3 targetVelocity,
            glm::vec3 gravity);

        // Return true if the constraint should be removed
        bool UpdateNoClipConstraint(JointState *joint, physx::PxRigidActor *actor0, physx::PxRigidActor *actor1);

        void UpdateJoints(
            ecs::Lock<ecs::Read<ecs::TransformTree, ecs::SceneProperties>, ecs::Write<ecs::PhysicsJoints>> lock,
            ecs::Entity entity,
            physx::PxRigidActor *actor,
            ecs::Transform transform);

        void ReleaseJoints(ecs::Entity entity, physx::PxRigidActor *actor);

        PhysxManager &manager;
    };
} // namespace sp
