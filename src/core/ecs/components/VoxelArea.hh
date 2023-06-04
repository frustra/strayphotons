#pragma once

#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct VoxelArea {
        glm::ivec3 extents = glm::ivec3(128);
    };

    static StructMetadata MetadataVoxelArea(typeid(VoxelArea),
        "voxel_area",
        "",
        StructField::New("extents", &VoxelArea::extents));
    static Component<VoxelArea> ComponentVoxelArea(MetadataVoxelArea);
} // namespace ecs
