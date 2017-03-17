#include "ecs/components/SlideDoor.hh"
#include "ecs/components/AnimateBlock.hh"

namespace ecs
{
	void SlideDoor::ValidateDoor() const
	{
		if (!left.Valid() || !right.Valid())
		{
			throw std::runtime_error("door panel is no longer valid");
		}
		if (!left.Has<AnimateBlock>() || !right.Has<AnimateBlock>())
		{
			throw std::runtime_error("door panel cannot be animated");
		}
	}

	bool SlideDoor::IsOpen()
	{
		ValidateDoor();
		ValidateDoor();
		auto lPanel = left.Get<AnimateBlock>();
		auto rPanel = right.Get<AnimateBlock>();
		return
			lPanel->curState == 1 && lPanel->nextState < 0
			&& rPanel->curState == 1 && rPanel->nextState < 0;

	}

	void SlideDoor::Open()
	{
		ValidateDoor();
		auto lPanel = left.Get<AnimateBlock>();
		auto rPanel = right.Get<AnimateBlock>();
		lPanel->AnimateToState(1);
		rPanel->AnimateToState(1);
	}

	void SlideDoor::Close()
	{
		ValidateDoor();
		auto lPanel = left.Get<AnimateBlock>();
		auto rPanel = right.Get<AnimateBlock>();
		lPanel->AnimateToState(0);
		rPanel->AnimateToState(0);
	}
}