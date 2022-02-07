#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class LaserSystem {
    public:
        LaserSystem(PhysxManager &manager);
        ~LaserSystem() {}

        void Frame(ecs::Lock<ecs::Read<ecs::Transform, ecs::LaserEmitter, ecs::Mirror>,
            ecs::Write<ecs::LaserLine, ecs::LaserSensor, ecs::SignalOutput>> lock);

    private:
        PhysxManager &manager;
    };
} // namespace sp