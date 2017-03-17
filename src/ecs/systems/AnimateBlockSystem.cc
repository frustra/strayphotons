#include "ecs/systems/AnimateBlockSystem.hh"
#include "ecs/components/AnimateBlock.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Renderable.hh"
#include "physx/PhysxUtils.hh"

#include <PxPhysicsAPI.h>

namespace ecs
{
	AnimateBlockSystem::AnimateBlockSystem(
		ecs::EntityManager &entities,
		sp::PhysxManager &physics)
		: entities(entities), physics(physics)
	{
	}

	AnimateBlockSystem::~AnimateBlockSystem() {}

	bool AnimateBlockSystem::Frame(float dtSinceLastFrame)
	{
		// safety net to avoid divide by 0
		if (dtSinceLastFrame <= std::numeric_limits<float>::epsilon())
		{
			return true;
		}

		for (auto ent : entities.EntitiesWith<AnimateBlock, Transform>())
		{
			auto block = ent.Get<AnimateBlock>();
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
				AnimateBlock::State &curState = block->states[block->curState];

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
				AnimateBlock::State &curState = block->states[block->curState];
				AnimateBlock::State &nextState = block->states[block->nextState];

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
			}
		}

		return true;
	}
}