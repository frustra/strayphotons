/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"

#include <glm/glm.hpp>

namespace ecs {
    struct VoxelArea {
        glm::ivec3 extents = glm::ivec3(128);
    };

    static Component<VoxelArea> ComponentVoxelArea(
        {typeid(VoxelArea), "voxel_area", "", StructField::New("extents", &VoxelArea::extents)});
} // namespace ecs
