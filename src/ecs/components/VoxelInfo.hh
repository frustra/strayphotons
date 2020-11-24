#pragma once

#include "graphics/GPUTypes.hh"

#include <ecs/Components.hh>
#include <glm/glm.hpp>

namespace ecs {
    struct VoxelArea {
        glm::vec3 min, max;
    };

    struct VoxelInfo {
        int gridSize = 0;
        float voxelSize, superSampleScale;
        glm::vec3 voxelGridCenter;
        glm::vec3 gridMin, gridMax;
        VoxelArea areas[sp::MAX_VOXEL_AREAS];
    };

    static Component<VoxelArea> ComponentVoxelArea("voxels"); // TODO: Rename this
    static Component<VoxelInfo> ComponentVoxelInfo("voxel_info");

    template<>
    bool Component<VoxelArea>::LoadEntity(Lock<AddRemove> lock, Tecs::Entity &dst, const picojson::value &src);

    Handle<VoxelInfo> UpdateVoxelInfoCache(Entity entity, int gridSize, float superSampleScale, EntityManager &em);
} // namespace ecs
