#pragma once

#include <glm/glm.hpp>

namespace ecs
{
	struct TriggerArea
	{
		glm::vec3 boundsMin, boundsMax;
		std::string command;
		bool triggered = false;
	};
}
