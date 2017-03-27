#include "ecs/systems/AnimationSystem.hh"
#include "ecs/components/Animation.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Renderable.hh"
#include "physx/PhysxUtils.hh"

#include "core/Logging.hh"

#include <PxPhysicsAPI.h>

namespace ecs
{
	AnimationSystem::AnimationSystem(
		ecs::EntityManager &entities)
		: entities(entities)
	{
	}

	AnimationSystem::~AnimationSystem() {}

	bool AnimationSystem::Frame(float dtSinceLastFrame)
	{
		for (auto ent : entities.EntitiesWith<Animation, Transform>())
		{
			auto animation = ent.Get<Animation>();
			auto transform = ent.Get<Transform>();

			if (animation->nextState < 0)
			{
				continue;
			}

			Assert((uint32)animation->nextState < animation->states.size(), "invalid next state");
			Assert((uint32)animation->curState < animation->states.size(), "invalid current state");
			Assert(animation->curState >= 0, "curState not set during an animation");

			auto &curState = animation->states[animation->curState];
			auto &nextState = animation->states[animation->nextState];

			glm::vec3 dPos = nextState.pos - curState.pos;
			glm::vec3 dScale = nextState.scale - curState.scale;

			float distToTarget = glm::length(transform->GetPosition() - nextState.pos);
			float completion = 1.0f - distToTarget / glm::length(dPos);

			float duration = animation->animationTimes[animation->nextState];
			float target = completion + dtSinceLastFrame / duration;

			if (distToTarget < 1e-4f || target >= 1.0f || std::isnan(target) || std::isinf(target))
			{
				animation->curState = animation->nextState;
				animation->nextState = -1;
				transform->SetPosition(nextState.pos);
				transform->SetScale(nextState.scale);

				if (ent.Has<ecs::Renderable>())
				{
					ent.Get<ecs::Renderable>()->hidden = nextState.hidden;
				}
			}
			else
			{
				transform->SetPosition(curState.pos + target * dPos);
				transform->SetScale(curState.scale + target * dScale);

				// ensure the entity is visible during the animation
				// when coming from a state that was hidden
				if (ent.Has<ecs::Renderable>())
				{
					ent.Get<ecs::Renderable>()->hidden = false;
				}
			}
		}

		return true;
	}
}