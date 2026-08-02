/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/components/VoxelData.hh"
#include "strayphotons/Async.hh"
#include "strayphotons/Utility.hh"

namespace ecs {
    struct VoxelData;
}

namespace sp {
    class Gltf;

    class MeshGenerator : public NonCopyable {
    public:
        virtual AsyncPtr<Gltf> GenerateMesh(const ecs::VoxelData &voxelData) = 0;
    };
} // namespace sp
