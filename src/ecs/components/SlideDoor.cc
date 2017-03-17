#include "ecs/components/SlideDoor.hh"
#include "ecs/components/Animation.hh"

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
}