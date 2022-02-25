#pragma once

#include "Common.hh"

namespace sp::vulkan::renderer {
    class SMAA {
    public:
        void AddPass(RenderGraph &graph);

    private:
        AsyncPtr<ImageView> areaTex, searchTex;
    };
} // namespace sp::vulkan::renderer
