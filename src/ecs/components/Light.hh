#pragma once

#include <glm/glm.hpp>

namespace ecs
{
	struct Light
	{
		float spotAngle, intensity, illuminance;
		glm::vec3 tint;
		glm::vec4 mapOffset;
	};
}
