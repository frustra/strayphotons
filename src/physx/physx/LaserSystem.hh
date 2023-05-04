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
            ecs::Write<ecs::LaserLine, ecs::LaserSensor, ecs::SignalOutput>> lock);

    private:
        PhysxManager &manager;

        const ecs::StringHandle laserColorRHandle = ecs::GetStringHandler().Get("laser_color_r");
        const ecs::StringHandle laserColorGHandle = ecs::GetStringHandler().Get("laser_color_g");
        const ecs::StringHandle laserColorBHandle = ecs::GetStringHandler().Get("laser_color_b");
        const ecs::StringHandle laserValueHandle = ecs::GetStringHandler().Get("value");
    };
} // namespace sp
