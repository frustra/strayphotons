#pragma once

#include <glm/glm.hpp>
#include "graphics/Graphics.hh"
#include <Ecs.hh>

namespace ecs
{
	struct View
	{
		View() { }
		View(glm::ivec2 extents) : extents(extents) { }

		// Required parameters.
		glm::ivec2 extents;
		glm::vec2 clip; // {near, far}
		float fov;

		// Optional parameters;
		glm::ivec2 offset = { 0, 0 };
		// TODO(any): Maybe remove color clear once we have interior spaces
		GLbitfield clearMode = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		glm::vec4 clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		bool stencil = false;
		bool blend = false;
		float skyIlluminance = 0.0f;
		float scale = 1.0f;

		// Updated automatically.
		float aspect;
		glm::mat4 projMat, invProjMat;
		glm::mat4 viewMat, invViewMat;
	};

	void ValidateView(ecs::Entity viewEntity);
	ecs::Handle<ecs::View> UpdateViewCache(ecs::Entity entity, float fov = 0.0);
}
