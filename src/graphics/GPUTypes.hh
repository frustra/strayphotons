#pragma once

#include "Common.hh"

#include <Ecs.hh>
#include <glm/glm.hpp>

namespace sp
{
	static const int MAX_LIGHTS = 16;
	static const int MAX_MIRRORS = 16;

	struct GLLightData {
		glm::vec3 position;
		float spotAngleCos;

		glm::vec3 tint;
		float intensity;

		glm::vec3 direction;
		float illuminance;

		glm::mat4 proj;
		glm::mat4 invProj;
		glm::mat4 view;
		glm::vec4 mapOffset;
		glm::vec2 clip;
		float padding[2];
	};

	static_assert(sizeof(GLLightData) == 4 * 17 * sizeof(float), "GLLightData size incorrect");

	struct GLMirrorData {
		glm::mat4 modelMat;
		glm::vec2 size;
		float padding[2];
	};

	static_assert(sizeof(GLMirrorData) == 4 * 5 * sizeof(float), "GLMirrorData size incorrect");

	int FillLightData(GLLightData *data, ecs::EntityManager &manager);
	int FillMirrorData(GLMirrorData *data, ecs::EntityManager &manager);
}
