#pragma once

#include <glm/glm.hpp>

#include <Ecs.hh>

namespace ecs
{
	struct Light
	{
		float spotAngle, intensity, illuminance;
		glm::vec3 tint;
		glm::vec4 mapOffset;
		int gelId;
		int lightId;
		bool on = true;
		ecs::Entity bulb;
	};
}
