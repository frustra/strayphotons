#pragma once

#include "graphics/vulkan/render_graph/RenderGraph.hh"

namespace sp::vulkan::render_graph {
    class SMAA {
    public:
        void AddPass(RenderGraph &graph);

    private:
        AsyncPtr<ImageView> areaTex, searchTex;
    };
} // namespace sp::vulkan::render_graph
