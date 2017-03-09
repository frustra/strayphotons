#pragma once

#include "Common.hh"
#include <Ecs.hh>

namespace ecs
{
	class LightSensor
	{
	public:
		struct Trigger
		{
			glm::vec3 illuminance;
			string oncmd, offcmd;

			bool operator()(glm::vec3 val)
			{
				return glm::all(glm::greaterThanEqual(val, illuminance));
			}
		};

		// Required parameters.
		glm::vec3 position = { 0, 0, 0 }; // In model space.
		glm::vec3 direction = { 0, 0, -1 }; // In model space.

		vector<Trigger> triggers;

		// Updated automatically.
		glm::vec3 illuminance;
		bool triggered = false;

		LightSensor() {}
		LightSensor(glm::vec3 p, glm::vec3 n) : position(p), direction(n) {}
	};
}
