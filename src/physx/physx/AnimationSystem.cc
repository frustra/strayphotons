#include "AnimationSystem.hh"

#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "physx/PhysxManager.hh"

namespace sp {
    AnimationSystem::AnimationSystem(PhysxManager &manager) : manager(manager) {}

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
                animation.currentState = animation.currentState + animation.PlayDirection();
                animation.targetState = newTargetState;
            }

            if (animation.targetState == animation.currentState) continue;

            Assert(animation.targetState < animation.states.size(), "invalid target state");
            Assert(animation.currentState < animation.states.size(), "invalid current state");

            auto nextStateIndex = animation.currentState + animation.PlayDirection();
            auto &currentState = animation.states[animation.currentState];
            auto &nextState = animation.states[nextStateIndex];

            glm::vec3 dPos = nextState.pos - currentState.pos;
            glm::vec3 dScale = nextState.scale - currentState.scale;

            float distance = glm::length(transform.GetPosition() - nextState.pos);
            float completion = 1.0f - distance / glm::length(dPos);

            float duration = animation.animationTimes[nextStateIndex];
            float nextCompletion = completion + (float)(manager.interval.count() / 1e9) / duration;

            if (distance < 1e-4f || nextCompletion >= 1.0f || std::isnan(nextCompletion) ||
                std::isinf(nextCompletion)) {
                animation.currentState = nextStateIndex;
                transform.SetPosition(nextState.pos);
                transform.SetScale(nextState.scale);
            } else {
                transform.SetPosition(currentState.pos + nextCompletion * dPos);
                transform.SetScale(currentState.scale + nextCompletion * dScale);
            }
        }
    }
} // namespace sp
