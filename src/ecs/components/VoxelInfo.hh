#pragma once

#include <glm/glm.hpp>

namespace ecs
{
	struct VoxelInfo
	{
		float voxelSize;
		glm::vec3 voxelGridCenter;
		// glm::vec3 voxelGridSize; TODO(xthexder)
	};
}
