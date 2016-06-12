#pragma once

#include <glm/glm.hpp>

namespace ECS
{
	struct View
	{
		// Required parameters.
		glm::ivec2 extents;
		glm::vec2 clip; // {near, far}
		float fov;

		// Optional parameters;
		glm::ivec2 offset = { 0, 0 };

		// Updated automatically.
		float aspect;
		glm::mat4 projMat, invProjMat;
		glm::mat4 viewMat, invViewMat;
	};
}