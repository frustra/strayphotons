#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include "ecs/components/VoxelInfo.hh"

namespace ecs
{
	Handle<VoxelInfo> UpdateVoxelInfoCache(Entity entity, int gridSize, float superSampleScale, EntityManager &em)
	{
		auto voxelInfo = entity.Get<VoxelInfo>();
		glm::vec3 gridMin = glm::vec3(0);
		glm::vec3 gridMax = glm::vec3(0);
		int areaIndex = 0;
		for (Entity ent : em.EntitiesWith<VoxelArea>())
		{
			auto area = ent.Get<VoxelArea>();
			if (!areaIndex)
			{
				gridMin = area->min;
				gridMax = area->max;
			}
			else
			{
				gridMin = glm::min(gridMin, area->min);
				gridMax = glm::max(gridMax, area->max);
			}
			voxelInfo->areas[areaIndex++] = *area;
		}

		voxelInfo->gridSize = gridSize;
		voxelInfo->superSampleScale = superSampleScale;
		voxelInfo->voxelGridCenter = (gridMin + gridMax) * glm::vec3(0.5);
		voxelInfo->voxelSize = glm::compMax(gridMax - gridMin) / gridSize;

		return voxelInfo;
	}
}
