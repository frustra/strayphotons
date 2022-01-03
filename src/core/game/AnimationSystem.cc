#include "AnimationSystem.hh"

#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"

namespace sp {
    bool AnimationSystem::Frame(double dtSinceLastFrame) {
        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::SignalOutput, ecs::SignalBindings, ecs::FocusLayer, ecs::FocusLock>,
            ecs::Write<ecs::Animation, ecs::Transform>>();
        for (auto ent : lock.EntitiesWith<ecs::Animation>()) {
            if (!ent.Has<ecs::Animation, ecs::Transform>(lock)) continue;

            auto &animation = ent.Get<ecs::Animation>(lock);
            auto &transform = ent.Get<ecs::Transform>(lock);

            double signalState = ecs::SignalBindings::GetSignal(lock, ent, "animation_state");
            size_t newTargetState = (size_t)(signalState + 0.5);
            if (newTargetState >= animation.states.size()) newTargetState = animation.states.size() - 1;
            if (animation.targetState != newTargetState) {
                animation.currentState = animation.targetState;
                animation.targetState = newTargetState;
            }

            if (animation.targetState == animation.currentState) continue;

            Assert(animation.targetState < animation.states.size(), "invalid target state");
            Assert(animation.currentState < animation.states.size(), "invalid current state");

            auto &currentState = animation.states[animation.currentState];
            auto &targetState = animation.states[animation.targetState];

            glm::vec3 dPos = targetState.pos - currentState.pos;
            glm::vec3 dScale = targetState.scale - currentState.scale;

            float distToTarget = glm::length(transform.GetPosition() - targetState.pos);
            float completion = 1.0f - distToTarget / glm::length(dPos);

            float duration = animation.animationTimes[animation.targetState];
            float target = completion + dtSinceLastFrame / duration;

            if (distToTarget < 1e-4f || target >= 1.0f || std::isnan(target) || std::isinf(target)) {
                animation.currentState = animation.targetState;
                transform.SetPosition(targetState.pos);
                transform.SetScale(targetState.scale);
            } else {
                transform.SetPosition(currentState.pos + target * dPos);
                transform.SetScale(currentState.scale + target * dScale);
            }
        }

        return true;
    }
} // namespace sp
