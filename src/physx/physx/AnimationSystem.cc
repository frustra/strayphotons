#include "AnimationSystem.hh"

#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"

namespace sp {
    AnimationSystem::AnimationSystem(PhysxManager &manager) : frameInterval(manager.interval.count() / 1e9) {}

    void AnimationSystem::Frame(ecs::Lock<ecs::ReadSignalsLock, ecs::Write<ecs::Animation, ecs::TransformTree>> lock) {
        ZoneScoped;
        for (auto ent : lock.EntitiesWith<ecs::Animation>()) {
            if (!ent.Has<ecs::Animation, ecs::TransformTree>(lock)) continue;

            auto &animation = ent.Get<ecs::Animation>(lock);
            if (animation.states.empty()) continue;

            animation.currentState = std::clamp(animation.currentState, 0.0, animation.states.size() - 1.0);
            double targetState = ecs::SignalBindings::GetSignal(lock, ent, "animation_target");
            auto state = animation.GetCurrNextState(targetState);
            if (targetState != animation.currentState) {
                auto &next = animation.states[state.next];

                double frameDelta = frameInterval / std::max(next.delay, frameInterval);
                if (frameDelta >= std::abs(targetState - animation.currentState)) {
                    animation.currentState = targetState;
                } else {
                    animation.currentState += state.direction * frameDelta;
                }
            }

            ecs::Animation::UpdateTransform(lock, ent);
        }
    }

    void AnimationSystem::UpdateSignals(ecs::Lock<ecs::Read<ecs::Animation>, ecs::Write<ecs::SignalOutput>> lock) {
        for (auto ent : lock.EntitiesWith<ecs::Animation>()) {
            if (!ent.Has<ecs::Animation, ecs::SignalOutput>(lock)) continue;
            const auto &anim = ent.Get<ecs::Animation>(lock);

            if (ent.Get<const ecs::SignalOutput>(lock).GetSignal("animation_state") != anim.currentState) {
                ent.Get<ecs::SignalOutput>(lock).SetSignal("animation_state", anim.currentState);
            }
        }
    }
} // namespace sp
