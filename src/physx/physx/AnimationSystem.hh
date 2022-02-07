#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class AnimationSystem {
    public:
        AnimationSystem(PhysxManager &manager);
        ~AnimationSystem() {}

        void Frame();

    private:
        double RoundToFrameInterval(double value) const;

        PhysxManager &manager;
        const double frameInterval;
    };
} // namespace sp
