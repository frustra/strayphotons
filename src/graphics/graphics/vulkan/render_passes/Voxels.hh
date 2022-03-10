#pragma once

#include "Common.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace sp::vulkan::renderer {
    class Voxels {
    public:
        Voxels(GPUScene &scene) : scene(scene) {}
        void LoadState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::VoxelArea, ecs::TransformSnapshot>> lock);

        void AddVoxelization(RenderGraph &graph);
        void AddDebugPass(RenderGraph &graph);

    private:
        GPUScene &scene;

        ecs::Transform voxelToWorld;
        glm::ivec3 voxelGridSize = glm::ivec3(1);
    };
} // namespace sp::vulkan::renderer
