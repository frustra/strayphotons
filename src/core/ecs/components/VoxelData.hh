/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Components.hh"
#include "ecs/StructMetadata.hh"
#include "strayphotons/HeapString.hh"
#include "strayphotons/HeapVector.hh"

#include <cstdint>
#include <glm/glm.hpp>

namespace ecs {
    struct VoxelData {
        sp::HeapString algorithm;
        glm::uvec3 extents = glm::uvec3(32);
        uint32_t seed = 0;
        sp::HeapVector<uint8_t> data;
    };

    static EntityComponent<VoxelData> ComponentVoxelData("voxel_data",
        "Storage for voxel data that can be turned into a mesh for use as a Renderable",
        StructField::New("algorithm", &VoxelData::algorithm),
        StructField::New("extents", &VoxelData::extents),
        StructField::New("seed", &VoxelData::seed),
        StructField::New("data", &VoxelData::data, FieldAction::AutoApply));
    template<>
    bool StructMetadata::Load<VoxelData>(VoxelData &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<VoxelData>(const EntityScope &scope,
        picojson::value &dst,
        const VoxelData &src,
        const VoxelData *def);
} // namespace ecs
