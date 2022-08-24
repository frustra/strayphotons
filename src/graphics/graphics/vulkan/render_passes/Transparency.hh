#pragma once

#include "Common.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace ecs {
    struct View;
}

namespace sp::vulkan::renderer {
    class Transparency {
    public:
        Transparency(GPUScene &scene) : scene(scene) {}

        void AddPass(RenderGraph &graph, const ecs::View &view);

    private:
        GPUScene &scene;
    };
} // namespace sp::vulkan::renderer
