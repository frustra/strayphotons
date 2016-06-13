#pragma once

#include <glm/glm.hpp>

namespace ECS
{
	struct Light
	{
		glm::vec3 tint;
		float illuminance;
	};
}
