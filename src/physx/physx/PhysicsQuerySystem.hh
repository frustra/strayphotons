#pragma once

namespace sp {
    class PhysxManager;

    class PhysicsQuerySystem {
    public:
        PhysicsQuerySystem(PhysxManager &manager);
        ~PhysicsQuerySystem() {}

        void Frame();

    private:
        PhysxManager &manager;
    };
} // namespace sp
