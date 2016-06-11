#pragma once

#include "Common.hh"

#include <unordered_map>
#include <vector>
#include <functional>

namespace ECS
{
	enum class ControlAction
	{
		MOVE_FORWARD,
		MOVE_BACKWARD,
		MOVE_LEFT,
		MOVE_RIGHT,
		MOVE_UP,
		MOVE_DOWN
	};

	struct HumanController
	{
		// map each action to a vector of glfw keys that could trigger it
		// (see InputManager.hh for converting a mouse button to one of these ints)
		std::unordered_map<ControlAction, std::vector<int>, sp::EnumHash> inputMap;

		// overrides ECS::Placement::rotate for an FPS-style orientation
		// until I figure out how to actually use quaternions
		float pitch;
		float yaw;
		float roll;
	};

}

