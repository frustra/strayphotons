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
