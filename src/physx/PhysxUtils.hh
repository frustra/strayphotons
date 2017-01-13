#pragma once

#include <glm/glm.hpp>
#include <foundation/PxVec3.h>

inline static physx::PxVec3 GlmVec3ToPxVec3(const glm::vec3 vector)
{
	return physx::PxVec3 (vector.x, vector.y, vector.z);
}

inline static glm::vec3 PxVec3ToGlmVec3P(const physx::PxVec3 vector)
{
	return glm::vec3 (vector.x, vector.y, vector.z);
}