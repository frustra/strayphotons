#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class ConstraintSystem {
    public:
        ConstraintSystem(PhysxManager &manager);
        ~ConstraintSystem() {}

        void Frame(ecs::Lock<ecs::Read<ecs::Transform>, ecs::Write<ecs::Physics>> lock);

    private:
        PhysxManager &manager;
    };
} // namespace sp
