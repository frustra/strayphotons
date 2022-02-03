#include "AnimationSystem.hh"

#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"

namespace sp {
    AnimationSystem::AnimationSystem(PhysxManager &manager)
        : manager(manager), frameInterval(manager.interval.count() / 1e9) {}

    double AnimationSystem::RoundToFrameInterval(double value) const {
        return frameInterval * std::round(value / frameInterval);
    }

    void AnimationSystem::Frame(
        ecs::Lock<ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>,
            ecs::Write<ecs::Animation, ecs::Transform>> lock) {
        ZoneScoped;
        for (auto ent : lock.EntitiesWith<ecs::Animation>()) {
            if (!ent.Has<ecs::Animation, ecs::Transform>(lock)) continue;

            auto &animation = ent.Get<ecs::Animation>(lock);
            if (animation.states.empty()) continue;

            auto &transform = ent.Get<ecs::Transform>(lock);

            double signalState = ecs::SignalBindings::GetSignal(lock, ent, "animation_state");
            size_t newTargetState = (size_t)(signalState + 0.5);
            if (newTargetState >= animation.states.size()) newTargetState = animation.states.size() - 1;

            if (animation.targetState != newTargetState) {
                auto oldNextState = animation.currentState + animation.PlayDirection();
                animation.currentState = oldNextState;
                animation.targetState = newTargetState;

                auto newNextState = animation.currentState + animation.PlayDirection();
                auto oldCompletion = 1.0 - animation.timeUntilNextState /
                                               RoundToFrameInterval(animation.animationTimes[oldNextState]);
                animation.timeUntilNextState = oldCompletion *
                                               RoundToFrameInterval(animation.animationTimes[newNextState]);
            }

            if (animation.targetState == animation.currentState) continue;

            Assert(animation.targetState < animation.states.size(), "invalid target state");
            Assert(animation.currentState < animation.states.size(), "invalid current state");

            auto playDirection = animation.PlayDirection();
            auto nextStateIndex = animation.currentState + playDirection;
            auto &currentState = animation.states[animation.currentState];
            auto &nextState = animation.states[nextStateIndex];

            double duration = RoundToFrameInterval(animation.animationTimes[nextStateIndex]);
            if (animation.timeUntilNextState <= 0) animation.timeUntilNextState = duration;
            animation.timeUntilNextState -= frameInterval;
            animation.timeUntilNextState = std::max(animation.timeUntilNextState, 0.0);

            float completion = 1.0 - animation.timeUntilNextState / duration;
            if (animation.interpolation == ecs::InterpolationMode::Linear) {
                glm::vec3 dPos = nextState.pos - currentState.pos;
                glm::vec3 dScale = nextState.scale - currentState.scale;
                transform.SetPosition(currentState.pos + completion * dPos);
                transform.SetScale(currentState.scale + completion * dScale);
            } else if (animation.interpolation == ecs::InterpolationMode::Cubic) {
                Assert(animation.tangents.size() == animation.states.size(), "invalid tangents");
                auto &prevTangent = animation.tangents[animation.currentState];
                auto &nextTangent = animation.tangents[nextStateIndex];
                float tangentScale = playDirection * duration;

                auto t = completion;
                auto t2 = t * t;
                auto t3 = t2 * t;
                auto av1 = 2 * t3 - 3 * t2 + 1;
                auto at1 = tangentScale * (t3 - 2 * t2 + t);
                auto av2 = -2 * t3 + 3 * t2;
                auto at2 = tangentScale * (t3 - t2);

                auto pos = av1 * currentState.pos + at1 * prevTangent.pos + av2 * nextState.pos + at2 * nextTangent.pos;
                transform.SetPosition(pos);

                auto scale = av1 * currentState.scale + at1 * prevTangent.scale + av2 * nextState.scale +
                             at2 * nextTangent.scale;
                transform.SetScale(scale);
            }

            if (animation.timeUntilNextState == 0) {
                animation.currentState = nextStateIndex;
                if (animation.interpolation == ecs::InterpolationMode::Step ||
                    animation.targetState == nextStateIndex) {
                    transform.SetPosition(nextState.pos);
                    transform.SetScale(nextState.scale);
                }
            }
        }
    }
} // namespace sp
