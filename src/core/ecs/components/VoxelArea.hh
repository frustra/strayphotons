#pragma once

#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct VoxelArea {
        glm::ivec3 extents = glm::ivec3(128);
    };

    static Component<VoxelArea> ComponentVoxelArea("voxel_area", ComponentField::New("extents", &VoxelArea::extents));
} // namespace ecs
