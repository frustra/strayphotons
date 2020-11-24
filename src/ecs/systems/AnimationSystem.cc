#include "ecs/systems/AnimationSystem.hh"

#include "core/Logging.hh"
#include "physx/PhysxUtils.hh"

#include <PxPhysicsAPI.h>
#include <ecs/Ecs.hh>
#include <ecs/EcsImpl.hh>

namespace ecs {
    AnimationSystem::AnimationSystem(ecs::ECS &ecs) : ecs(ecs) {}

    AnimationSystem::~AnimationSystem() {}

    bool AnimationSystem::Frame(float dtSinceLastFrame) {
        auto lock = ecs.StartTransaction<Write<Animation, Transform, Renderable>>();
        for (auto ent : lock.EntitiesWith<Animation>()) {
            if (!ent.Has<Animation, Transform>(lock)) continue;

            auto &animation = ent.Get<Animation>(lock);
            auto &transform = ent.Get<Transform>(lock);

            if (animation.prevState < 0 || animation.curState < 0 || animation.curState == animation.prevState) {
                continue;
            }

            sp::Assert(animation.curState < animation.states.size(), "invalid current state");
            sp::Assert(animation.prevState < animation.states.size(), "invalid previous state");

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

                if (ent.Has<Renderable>(lock)) { ent.Get<Renderable>(lock).hidden = curState.hidden; }
            } else {
                transform.SetPosition(prevState.pos + target * dPos);
                transform.SetScale(prevState.scale + target * dScale);

                // ensure the entity is visible during the animation
                // when coming from a state that was hidden
                if (ent.Has<Renderable>(lock)) { ent.Get<Renderable>(lock).hidden = false; }
            }
        }

        return true;
    }
} // namespace ecs
