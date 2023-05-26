#pragma once

#include "ecs/Ecs.hh"
#include "ecs/SignalExpression.hh"

namespace sp {
    class PhysxManager;

    class AnimationSystem {
    public:
        AnimationSystem(PhysxManager &manager);
        ~AnimationSystem() {}

        void Frame(ecs::Lock<ecs::ReadSignalsLock,
            ecs::Read<ecs::Animation, ecs::LightSensor, ecs::LaserSensor>,
            ecs::Write<ecs::Signals, ecs::TransformTree>> lock);

    private:
        const double frameInterval;
    };
} // namespace sp
