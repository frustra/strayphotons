#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class AnimationSystem {
    public:
        AnimationSystem(PhysxManager &manager);
        ~AnimationSystem() {}

        void Frame(ecs::Lock<ecs::ReadSignalsLock, ecs::Write<ecs::Animation, ecs::TransformTree>> lock);

    private:
        double RoundToFrameInterval(double value) const;

        const double frameInterval;
    };
} // namespace sp
