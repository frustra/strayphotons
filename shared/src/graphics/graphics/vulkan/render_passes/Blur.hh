/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"

namespace sp::vulkan::renderer {
    ResourceID AddGaussianBlur1D(RenderGraph &graph,
        ResourceID sourceID,
        glm::ivec2 direction,
        uint32 downsample = 1,
        float scale = 1.0f,
        float clip = FLT_MAX);
} // namespace sp::vulkan::renderer
