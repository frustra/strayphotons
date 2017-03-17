#pragma once

#include "Common.hh"
#include <glm/glm.hpp>

namespace ecs
{
	class Animation
	{
	public:
		void AnimateToState(uint32 i);

		struct State
		{
			glm::vec3 scale;
			glm::vec3 pos;

			// if model should be hidden upon reaching this state
			bool hidden = false;
		};

		vector<State> states;
		int curState = -1;
		int nextState = -1;
		float timeLeft = 0;

		/**
		 * the time it takes to animate to the given state
		 */
		vector<float> animationTimes;
	};
}