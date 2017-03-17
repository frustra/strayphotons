#include "ecs/systems/AnimationSystem.hh"
#include "ecs/components/Animation.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Renderable.hh"
#include "physx/PhysxUtils.hh"

#include <PxPhysicsAPI.h>

namespace ecs
{
	AnimationSystem::AnimationSystem(
		ecs::EntityManager &entities,
		sp::PhysxManager &physics)
		: entities(entities), physics(physics)
	{
	}

	AnimationSystem::~AnimationSystem() {}

	bool AnimationSystem::Frame(float dtSinceLastFrame)
	{
		// safety net to avoid divide by 0
		if (dtSinceLastFrame <= std::numeric_limits<float>::epsilon())
		{
			return true;
		}

		for (auto ent : entities.EntitiesWith<Animation, Transform>())
		{
			auto animation = ent.Get<Animation>();
			auto transform = ent.Get<Transform>();

			if (animation->nextState < 0)
			{
				continue;
			}

			Assert((uint32)animation->nextState < animation->states.size(),
				"invalid next state");
			Assert((uint32)animation->curState < animation->states.size(),
				"invalid current state");
			Assert(animation->curState >= 0, "curState not set during an animation");
			Assert(animation->timeLeft >= 0, "negative animation time");

			if (dtSinceLastFrame > animation->timeLeft)
			{
				animation->timeLeft = 0;
				animation->curState = animation->nextState;
				animation->nextState = -1;
				Animation::State &curState = animation->states[animation->curState];

				transform->SetPosition(curState.pos);
				transform->SetScale(curState.scale);

				if (ent.Has<ecs::Renderable>())
				{
					ent.Get<ecs::Renderable>()->hidden = curState.hidden;
				}
			}
			else
			{
				animation->timeLeft -= dtSinceLastFrame;
				Animation::State &curState =
					animation->states[animation->curState];
				Animation::State &nextState =
					animation->states[animation->nextState];

				float duration = animation->animationTimes[animation->nextState];
				float completion = 1 - (animation->timeLeft / duration);

				glm::vec3 dPos = nextState.pos - curState.pos;
				glm::vec3 dScale = nextState.scale - curState.scale;

				transform->SetPosition(curState.pos + completion * dPos);
				transform->SetScale(curState.scale + completion * dScale);

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