#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class LaserSystem {
    public:
        LaserSystem(PhysxManager &manager);
        ~LaserSystem() {}

        void Frame(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::LaserEmitter, ecs::OpticalElement>,
            ecs::Write<ecs::LaserLine, ecs::LaserSensor, ecs::SignalOutput>> lock);

    private:
        PhysxManager &manager;
    };
} // namespace sp
