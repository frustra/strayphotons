#pragma once

#include "Common.hh"
#include <Ecs.hh>

namespace ecs
{
	class LightSensor
	{
	public:
		// Optional parameters.
		float threshold = 0.0f;

		// Updated automatically.
		glm::vec3 illuminance;
		bool triggered = false;

		LightSensor() {}
		LightSensor(float threshold) : threshold(threshold) {}
	};
}
