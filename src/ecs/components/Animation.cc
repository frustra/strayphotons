#include "ecs/components/Animation.hh"

#include <sstream>

namespace ecs
{
	void Animation::AnimateToState(uint i)
	{
		if (i > states.size())
		{
			std::stringstream ss;
			ss << "\"" << i << "\" is an invalid state for this Animation with "
			   << states.size() << " states";
			throw std::runtime_error(ss.str());
		}

		nextState = i;
		timeLeft = animationTimes.at(i);
	}
}