#pragma once

#include "ecs/Ecs.hh"
#include "ecs/SignalExpression.hh"

namespace sp {
    class PhysxManager;

    class AnimationSystem {
    public:
        AnimationSystem(PhysxManager &manager);
        ~AnimationSystem() {}

        void Frame(ecs::Lock<ecs::ReadSignalsLock, ecs::Write<ecs::Animation, ecs::TransformTree>> lock);
        void UpdateSignals(ecs::Lock<ecs::Read<ecs::Animation>, ecs::Write<ecs::SignalOutput>> lock);

    private:
        const double frameInterval;
    };
} // namespace sp
