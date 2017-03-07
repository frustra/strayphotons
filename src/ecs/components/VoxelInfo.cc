#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include "ecs/components/VoxelInfo.hh"

namespace ecs
{
	Handle<VoxelInfo> UpdateVoxelInfoCache(Entity entity, int gridSize, float superSampleScale)
	{
		auto voxelInfo = entity.Get<VoxelInfo>();

		voxelInfo->gridSize = gridSize;
		voxelInfo->superSampleScale = superSampleScale;
		voxelInfo->voxelGridCenter = (voxelInfo->gridMin + voxelInfo->gridMax) * glm::vec3(0.5);
		voxelInfo->voxelSize = glm::compMax(voxelInfo->gridMax - voxelInfo->gridMin) / voxelInfo->gridSize;

		return voxelInfo;
	}
}
