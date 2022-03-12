#pragma once

#include "core/Config.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct VoxelArea {
        glm::ivec3 extents = glm::ivec3(128);
    };

    static Component<VoxelArea> ComponentVoxelArea("voxel_area");

    template<>
    bool Component<VoxelArea>::Load(ScenePtr scenePtr, VoxelArea &dst, const picojson::value &src);
} // namespace ecs
