#pragma once

#include "ecs/Ecs.hh"

namespace sp {
    class PhysxManager;

    class AnimationSystem {
    public:
        AnimationSystem(PhysxManager &manager);
        ~AnimationSystem() {}

        void Frame(
            ecs::Lock<ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>,
                ecs::Write<ecs::Animation, ecs::TransformTarget>> lock);

    private:
        double RoundToFrameInterval(double value) const;

        PhysxManager &manager;
        const double frameInterval;
    };
} // namespace sp
