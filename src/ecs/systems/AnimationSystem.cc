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
			auto block = ent.Get<Animation>();
			auto transform = ent.Get<Transform>();

			if (block->nextState < 0)
			{
				continue;
			}

			Assert((uint)block->nextState < block->states.size(),
				"invalid next state");
			Assert((uint)block->curState < block->states.size(),
				"invalid current state");
			Assert(block->curState >= 0, "curState not set during an animation");
			Assert(block->timeLeft >= 0, "negative animation time");

			if (dtSinceLastFrame > block->timeLeft)
			{
				block->timeLeft = 0;
				block->curState = block->nextState;
				block->nextState = -1;
				Animation::State &curState = block->states[block->curState];

				transform->SetPosition(curState.pos);
				transform->SetScale(curState.scale);

				if (ent.Has<ecs::Renderable>())
				{
					ent.Get<ecs::Renderable>()->hidden = curState.hidden;
				}
			}
			else
			{
				block->timeLeft -= dtSinceLastFrame;
				Animation::State &curState = block->states[block->curState];
				Animation::State &nextState = block->states[block->nextState];

				float duration = block->animationTimes[block->nextState];
				float completion = 1 - (block->timeLeft / duration);

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

				Logf("animating %f", completion);
			}
		}

		return true;
	}
}