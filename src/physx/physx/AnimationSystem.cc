#include "AnimationSystem.hh"

#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"

namespace sp {
    AnimationSystem::AnimationSystem(PhysxManager &manager) : frameInterval(manager.interval.count() / 1e9) {}

    void AnimationSystem::Frame(
        ecs::Lock<ecs::ReadSignalsLock, ecs::Read<ecs::Animation>, ecs::Write<ecs::SignalOutput, ecs::TransformTree>>
            lock) {
        ZoneScoped;
        for (auto ent : lock.EntitiesWith<ecs::Animation>()) {
            if (!ent.Has<ecs::Animation>(lock)) continue;
            auto &animation = ent.Get<ecs::Animation>(lock);
            if (animation.states.empty()) continue;

            double currentState = ecs::SignalBindings::GetSignal(lock, ent, "animation_state");
            double targetState = ecs::SignalBindings::GetSignal(lock, ent, "animation_target");
            double originalState = currentState;
            currentState = std::clamp(currentState, 0.0, animation.states.size() - 1.0);
            targetState = std::clamp(targetState, 0.0, animation.states.size() - 1.0);
            auto state = animation.GetCurrNextState(currentState, targetState);
            if (targetState != currentState) {
                auto &next = animation.states[state.next];

                double frameDelta = frameInterval / std::max(next.delay, frameInterval);
                if (frameDelta >= std::abs(targetState - currentState)) {
                    currentState = targetState;
                } else {
                    currentState += state.direction * frameDelta;
                }
            }

            ecs::Animation::UpdateTransform(lock, ent);

            if (ent.Has<ecs::SignalOutput>(lock)) {
                if (originalState != currentState) {
                    auto &signalOutput = ent.Get<ecs::SignalOutput>(lock);
                    signalOutput.SetSignal("animation_state", currentState);
                }
            } else {
                Warnf("Entity %s has animation component but no signal output", ecs::ToString(lock, ent));
            }
        }
    }
} // namespace sp
