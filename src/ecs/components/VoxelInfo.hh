#pragma once

#include <glm/glm.hpp>

namespace ecs
{
	struct VoxelInfo
	{
		int gridSize;
		float voxelSize, superSampleScale;
		glm::vec3 voxelGridCenter;
		glm::vec3 gridMin, gridMax;
	};
}
