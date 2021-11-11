#pragma once

#include "core/Config.hh"
#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct VoxelArea {
        glm::vec3 min, max;
    };

    static Component<VoxelArea> ComponentVoxelArea("voxels"); // TODO: Rename this

    template<>
    bool Component<VoxelArea>::Load(sp::Scene *scene, VoxelArea &dst, const picojson::value &src);
} // namespace ecs
