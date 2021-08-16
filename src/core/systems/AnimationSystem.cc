#include "AnimationSystem.hh"

#include "core/Logging.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"

namespace sp {
    AnimationSystem::AnimationSystem(ecs::ECS &ecs) : ecs(ecs) {}

    AnimationSystem::~AnimationSystem() {}

    bool AnimationSystem::Frame(float dtSinceLastFrame) {
        auto lock = ecs.StartTransaction<ecs::Write<ecs::Animation, ecs::Transform>>();
        for (auto ent : lock.EntitiesWith<ecs::Animation>()) {
            if (!ent.Has<ecs::Animation, ecs::Transform>(lock)) continue;

            auto &animation = ent.Get<ecs::Animation>(lock);
            auto &transform = ent.Get<ecs::Transform>(lock);

            if (animation.curState == animation.prevState) continue;

            Assert(animation.curState < animation.states.size(), "invalid current state");
            Assert(animation.prevState < animation.states.size(), "invalid previous state");

            auto &prevState = animation.states[animation.prevState];
            auto &curState = animation.states[animation.curState];

            glm::vec3 dPos = curState.pos - prevState.pos;
            glm::vec3 dScale = curState.scale - prevState.scale;

            float distToTarget = glm::length(transform.GetPosition() - curState.pos);
            float completion = 1.0f - distToTarget / glm::length(dPos);

            float duration = animation.animationTimes[animation.curState];
            float target = completion + dtSinceLastFrame / duration;

            if (distToTarget < 1e-4f || target >= 1.0f || std::isnan(target) || std::isinf(target)) {
                animation.prevState = animation.curState;
                transform.SetPosition(curState.pos);
                transform.SetScale(curState.scale);
            } else {
                transform.SetPosition(prevState.pos + target * dPos);
                transform.SetScale(prevState.scale + target * dScale);
            }
        }

        return true;
    }
} // namespace sp
