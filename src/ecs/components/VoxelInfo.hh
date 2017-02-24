#pragma once

#include <glm/glm.hpp>
#include "Ecs.hh"

namespace ecs
{
	struct VoxelInfo
	{
		int gridSize = 0;
		float voxelSize, superSampleScale;
		glm::vec3 voxelGridCenter;
		glm::vec3 gridMin, gridMax;
	};

	Handle<VoxelInfo> UpdateVoxelInfoCache(Entity entity, int gridSize, float superSampleScale);
}
