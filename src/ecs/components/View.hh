#pragma once

#include <glm/glm.hpp>

namespace ecs
{
	struct View
	{
		// Required parameters.
		glm::ivec2 extents;
		glm::vec2 clip; // {near, far}
		float fov;

		// Optional parameters;
		glm::ivec2 offset = { 0, 0 };
		GLbitfield clearMode = GL_DEPTH_BUFFER_BIT;
		glm::vec4 clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		// Updated automatically.
		float aspect;
		glm::mat4 projMat, invProjMat;
		glm::mat4 viewMat, invViewMat;
	};
}