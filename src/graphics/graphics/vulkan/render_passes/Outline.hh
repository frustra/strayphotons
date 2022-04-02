#pragma once

#include "Common.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace sp::vulkan::renderer {
    void AddOutlines(RenderGraph &graph, GPUScene &scene);
} // namespace sp::vulkan::renderer
