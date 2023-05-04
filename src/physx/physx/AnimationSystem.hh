#pragma once

#include "ecs/Ecs.hh"
#include "ecs/SignalExpression.hh"
#include "ecs/StringHandle.hh"

namespace sp {
    class PhysxManager;

    class AnimationSystem {
    public:
        AnimationSystem(PhysxManager &manager);
        ~AnimationSystem() {}

        void Frame(ecs::Lock<ecs::ReadSignalsLock,
            ecs::Read<ecs::Animation>,
            ecs::Write<ecs::SignalOutput, ecs::TransformTree>> lock);

    private:
        const double frameInterval;

        const ecs::StringHandle animationStateHandle = ecs::GetStringHandler().Get("animation_state");
        const ecs::StringHandle animationTargetHandle = ecs::GetStringHandler().Get("animation_target");
    };
} // namespace sp
