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

        void Frame(ecs::Lock<ecs::Read<ecs::TransformTree,
                ecs::CharacterController,
                ecs::Physics,
                ecs::PhysicsJoints,
                ecs::SceneProperties>> lock);

    private:
        bool UpdateForceConstraint(physx::PxRigidActor *actor,
            JointState *joint,
            ecs::Transform transform,
            ecs::Transform targetTransform,
            glm::vec3 targetVelocity,
            glm::vec3 gravity);

        void UpdateJoints(ecs::Lock<ecs::Read<ecs::TransformTree, ecs::PhysicsJoints, ecs::SceneProperties>> lock,
            ecs::Entity entity,
            physx::PxRigidActor *actor,
            ecs::Transform transform);

        void ReleaseJoints(ecs::Entity entity, physx::PxRigidActor *actor);

        PhysxManager &manager;
    };
} // namespace sp
