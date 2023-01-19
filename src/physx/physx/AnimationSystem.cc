#include "AnimationSystem.hh"

#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"

namespace sp {
    AnimationSystem::AnimationSystem(PhysxManager &manager) : frameInterval(manager.interval.count() / 1e9) {}

    double AnimationSystem::RoundToFrameInterval(double value) const {
        return frameInterval * std::round(value / frameInterval);
    }

    void AnimationSystem::Frame(ecs::Lock<ecs::ReadSignalsLock, ecs::Write<ecs::Animation, ecs::TransformTree>> lock) {
        ZoneScoped;
        for (auto ent : lock.EntitiesWith<ecs::Animation>()) {
            if (!ent.Has<ecs::Animation, ecs::TransformTree>(lock)) continue;

            auto &animation = ent.Get<ecs::Animation>(lock);
            if (animation.states.empty()) continue;

            auto &transform = ent.Get<ecs::TransformTree>(lock);

            double signalState = ecs::SignalBindings::GetSignal(lock, ent, "animation_state");
            size_t newTargetState = (size_t)(signalState + 0.5);
            if (newTargetState >= animation.states.size()) newTargetState = animation.states.size() - 1;

            animation.currentState = std::clamp<size_t>(animation.currentState, 0, animation.states.size() - 1);
            animation.targetState = std::clamp<size_t>(animation.targetState, 0, animation.states.size() - 1);
            if (animation.targetState != newTargetState) {
                auto oldNextState = animation.currentState + animation.PlayDirection();
                animation.currentState = oldNextState;
                animation.targetState = newTargetState;

                auto newNextState = animation.currentState + animation.PlayDirection();
                auto oldCompletion = 1.0 - animation.timeUntilNextState /
                                               RoundToFrameInterval(animation.states[oldNextState].delay);
                animation.timeUntilNextState = oldCompletion *
                                               RoundToFrameInterval(animation.states[newNextState].delay);
            }

            if (animation.targetState == animation.currentState) continue;

            Assert(animation.targetState < animation.states.size(), "invalid target state");
            Assert(animation.currentState < animation.states.size(), "invalid current state");

            auto playDirection = animation.PlayDirection();
            auto nextStateIndex = animation.currentState + playDirection;
            auto &currentState = animation.states[animation.currentState];
            auto &nextState = animation.states[nextStateIndex];

            double duration = RoundToFrameInterval(nextState.delay);
            if (animation.timeUntilNextState <= 0) animation.timeUntilNextState = duration;
            animation.timeUntilNextState -= frameInterval;
            animation.timeUntilNextState = std::max(animation.timeUntilNextState, 0.0);

            float completion = 1.0 - animation.timeUntilNextState / duration;
            if (animation.interpolation == ecs::InterpolationMode::Linear) {
                glm::vec3 dPos = nextState.pos - currentState.pos;
                glm::vec3 dScale = nextState.scale - currentState.scale;
                transform.pose.SetPosition(currentState.pos + completion * dPos);
                transform.pose.SetScale(currentState.scale + completion * dScale);
            } else if (animation.interpolation == ecs::InterpolationMode::Cubic) {
                float tangentScale = playDirection * duration;

                // auto t = completion;
                // auto t2 = t * t;
                // auto t3 = t2 * t;
                // auto av1 = 2 * t3 - 3 * t2 + 1;
                // auto at1 = tangentScale * (t3 - 2 * t2 + t);
                // auto av2 = -2 * t3 + 3 * t2;
                // auto at2 = tangentScale * (t3 - t2);

                auto pos = CubicBlend(completion,
                    currentState.pos,
                    tangentScale * currentState.tangentPos,
                    nextState.pos,
                    tangentScale * nextState.tangentPos);
                // auto pos = av1 * currentState.pos + at1 * currentState.tangentPos + av2 * nextState.pos +
                //            at2 * nextState.tangentPos;
                transform.pose.SetPosition(pos);

                auto scale = CubicBlend(completion,
                    currentState.scale,
                    tangentScale * currentState.tangentScale,
                    nextState.scale,
                    tangentScale * nextState.tangentScale);
                // auto scale = av1 * currentState.scale + at1 * currentState.tangentScale + av2 * nextState.scale +
                //              at2 * nextState.tangentScale;
                transform.pose.SetScale(scale);
            }

            if (animation.timeUntilNextState == 0) {
                animation.currentState = nextStateIndex;
                if (animation.interpolation == ecs::InterpolationMode::Step ||
                    animation.targetState == nextStateIndex) {
                    transform.pose.SetPosition(nextState.pos);
                    transform.pose.SetScale(nextState.scale);
                }
            }
        }
    }
} // namespace sp
