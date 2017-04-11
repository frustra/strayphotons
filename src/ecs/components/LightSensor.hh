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
			float onSignal = 1.0f;
			float offSignal = 0.0f;

			bool operator()(glm::vec3 val)
			{
				return glm::all(glm::greaterThanEqual(val, illuminance));
			}
		};

		// Required parameters.
		glm::vec3 position = { 0, 0, 0 }; // In model space.
		glm::vec3 direction = { 0, 0, -1 }; // In model space.
		glm::vec3 onColor = { 0, 1, 0 };
		glm::vec3 offColor = { 0, 0, 0 };

		vector<Trigger> triggers;

		// Updated automatically.
		glm::vec3 illuminance;
		bool triggered = false;

		LightSensor() {}
		LightSensor(glm::vec3 p, glm::vec3 n) : position(p), direction(n) {}
	};
}
