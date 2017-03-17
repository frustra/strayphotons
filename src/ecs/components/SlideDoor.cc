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

	bool SlideDoor::IsOpen()
	{
		ValidateDoor();
		auto lPanel = left.Get<Animation>();
		auto rPanel = right.Get<Animation>();
		return
			lPanel->curState == 1 && lPanel->nextState < 0
			&& rPanel->curState == 1 && rPanel->nextState < 0;
	}

	void SlideDoor::Open()
	{
		ValidateDoor();
		if (IsOpen()) return;

		auto lPanel = left.Get<Animation>();
		auto rPanel = right.Get<Animation>();
		lPanel->AnimateToState(1);
		rPanel->AnimateToState(1);
	}

	void SlideDoor::Close()
	{
		ValidateDoor();
		if (!IsOpen()) return;

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
			if (panel == left)
			{
				animatePos = panelPos + glm::vec3(panelWidth, 0, 0);
			}
			else
			{
				animatePos = panelPos + glm::vec3(-panelWidth, 0, 0);
			}

			animation->states.resize(2);
			animation->animationTimes.resize(2);

			// closed
			Animation::State closeState;
			closeState.scale = glm::vec3(1, 1, 1);
			closeState.pos = panelPos;
			animation->states[0] = closeState;
			animation->animationTimes[0] = this->openTime;

			// open
			Animation::State openState;
			openState.scale = glm::vec3(1, 1, 1);
			openState.pos = animatePos;
			animation->states[1] = openState;
			animation->animationTimes[1] = this->openTime;

			animation->curState = 0;
		}
	}
}