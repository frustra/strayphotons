#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class ConstraintSystem {
    public:
        ConstraintSystem(PhysxManager &manager);
        ~ConstraintSystem() {}

        void Frame(ecs::Lock<ecs::Read<ecs::TransformTree, ecs::CharacterController>, ecs::Write<ecs::Physics>> lock);

    private:
        void HandleForceLimitConstraint(ecs::Physics &physics,
            ecs::Transform transform,
            ecs::Transform targetTransform,
            glm::vec3 targetVelocity);

        PhysxManager &manager;
    };
} // namespace sp
