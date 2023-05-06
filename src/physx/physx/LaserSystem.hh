#pragma once

#include "ecs/Ecs.hh"
#include "ecs/SignalExpression.hh"

namespace sp {
    class PhysxManager;

    class LaserSystem {
    public:
        LaserSystem(PhysxManager &manager);
        ~LaserSystem() {}

        void Frame(ecs::Lock<ecs::ReadSignalsLock,
            ecs::Read<ecs::TransformSnapshot, ecs::LaserEmitter, ecs::OpticalElement>,
            ecs::Write<ecs::LaserLine, ecs::LaserSensor, ecs::Signals>> lock);

    private:
        PhysxManager &manager;
    };
} // namespace sp
