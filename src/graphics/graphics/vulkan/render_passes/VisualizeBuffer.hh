#pragma once

#include "Common.hh"

namespace sp::vulkan::renderer {
    ResourceID VisualizeBuffer(RenderGraph &graph, ResourceID sourceID, uint32 arrayLayer = ~0u);
} // namespace sp::vulkan::renderer
