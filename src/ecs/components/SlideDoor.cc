#include "ecs/components/SlideDoor.hh"
#include "ecs/components/Animation.hh"
#include "ecs/components/Transform.hh"

namespace ecs
{
	void SlideDoor::ValidateDoor() const
	{
		if (!left.Valid() || !right.Valid())
		{
			throw std::runtime_error("door panel is no longer valid");
		}
		if (!left.Has<Animation>() || !right.Has<Animation>())
		{
			throw std::runtime_error("door panel cannot be animated");
		}
	}

	SlideDoor::State SlideDoor::GetState()
	{
		ValidateDoor();
		auto lPanel = left.Get<Animation>();
		auto rPanel = right.Get<Animation>();

		SlideDoor::State state;
		if (lPanel->curState == 1 && lPanel->nextState < 0)
		{
			state = SlideDoor::State::OPENED;
		}
		else if (lPanel->curState == 1 && lPanel->nextState >= 0)
		{
			state = SlideDoor::State::CLOSING;
		}
		else if (lPanel->curState == 0 && lPanel->nextState < 0)
		{
			state = SlideDoor::State::CLOSED;
		}
		else
		{
			state = SlideDoor::State::OPENING;
		}

		return state;
	}

	void SlideDoor::Open()
	{
		ValidateDoor();

		auto lPanel = left.Get<Animation>();
		auto rPanel = right.Get<Animation>();
		lPanel->AnimateToState(1);
		rPanel->AnimateToState(1);
	}

	void SlideDoor::Close()
	{
		ValidateDoor();

		auto lPanel = left.Get<Animation>();
		auto rPanel = right.Get<Animation>();
		lPanel->AnimateToState(0);
		rPanel->AnimateToState(0);
	}

	void SlideDoor::ApplyParams()
	{
		for (Entity panel : vector<Entity>({left, right}))
		{
			if (!panel.Valid() || !panel.Has<Transform>()
				|| !panel.Has<Animation>())
			{
				continue;
			}

			auto transform = panel.Get<Transform>();
			auto animation = panel.Get<Animation>();
			glm::vec3 panelPos = transform->GetPosition();
			glm::vec3 animatePos;
			float panelWidth = this->width / 2;

			glm::vec3 leftDir = glm::normalize(glm::cross(this->forward, transform->GetUp()));

			if (panel == left)
			{
				animatePos = panelPos + panelWidth * leftDir;
			}
			else
			{
				animatePos = panelPos - panelWidth * leftDir;
			}

			animation->states.resize(2);
			animation->animationTimes.resize(2);

			// closed
			Animation::State closeState;
			closeState.scale = transform->GetScaleVec();
			closeState.pos = panelPos;
			animation->states[0] = closeState;
			animation->animationTimes[0] = this->openTime;

			// open
			Animation::State openState;
			openState.scale = transform->GetScaleVec();
			openState.pos = animatePos;
			animation->states[1] = openState;
			animation->animationTimes[1] = this->openTime;

			animation->curState = 0;
		}
	}
}