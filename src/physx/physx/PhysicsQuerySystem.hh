#pragma once

namespace sp {
    class PhysxManager;

    class PhysicsQuerySystem {
    public:
        PhysicsQuerySystem(PhysxManager &manager);
        ~PhysicsQuerySystem() {}

        void Frame(ecs::Lock<ecs::Read<ecs::TransformSnapshot>, ecs::Write<ecs::PhysicsQuery>> lock);

    private:
        PhysxManager &manager;
    };
} // namespace sp
