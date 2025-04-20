/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"

namespace sp::vulkan::renderer {
    class Voxels;

    class Radiance {
    public:
        Radiance(Voxels &voxels) : voxels(voxels) {}

        void AddFlatlandRC(RenderGraph &graph);

    private:
        Voxels &voxels;
    };
} // namespace sp::vulkan::renderer
