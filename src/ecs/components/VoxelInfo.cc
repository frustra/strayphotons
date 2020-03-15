#include "ecs/components/VoxelInfo.hh"

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>

#include <picojson/picojson.h>
#include <assets/AssetHelpers.hh>

namespace ecs
{
	template<>
	bool Component<VoxelArea>::LoadEntity(Entity &dst, picojson::value &src)
	{
		auto voxelArea = dst.Assign<VoxelArea>();
		for (auto param : src.get<picojson::object>())
		{
			if (param.first == "min")
			{
				voxelArea->min = sp::MakeVec3(param.second);
			}
			else if (param.first == "max")
			{
				voxelArea->max = sp::MakeVec3(param.second);
			}
		}
		return true;
	}

	Handle<VoxelInfo> UpdateVoxelInfoCache(Entity entity, int gridSize, float superSampleScale, EntityManager &em)
	{
		auto voxelInfo = entity.Get<VoxelInfo>();
		voxelInfo->gridMin = glm::vec3(0);
		voxelInfo->gridMax = glm::vec3(0);
		int areaIndex = 0;
		for (Entity ent : em.EntitiesWith<VoxelArea>())
		{
			if (areaIndex >= sp::MAX_VOXEL_AREAS) break;

			auto area = ent.Get<VoxelArea>();
			if (!areaIndex)
			{
				voxelInfo->gridMin = area->min;
				voxelInfo->gridMax = area->max;
			}
			else
			{
				voxelInfo->gridMin = glm::min(voxelInfo->gridMin, area->min);
				voxelInfo->gridMax = glm::max(voxelInfo->gridMax, area->max);
			}
			voxelInfo->areas[areaIndex++] = *area;
		}
		for (; areaIndex < sp::MAX_VOXEL_AREAS; areaIndex++)
		{
			voxelInfo->areas[areaIndex] = VoxelArea{glm::vec3(0), glm::vec3(-1)};
		}

		voxelInfo->gridSize = gridSize;
		voxelInfo->superSampleScale = superSampleScale;
		voxelInfo->voxelGridCenter = (voxelInfo->gridMin + voxelInfo->gridMax) * glm::vec3(0.5);
		voxelInfo->voxelSize = glm::compMax(voxelInfo->gridMax - voxelInfo->gridMin + glm::vec3(0.1)) / gridSize;

		return voxelInfo;
	}
}
