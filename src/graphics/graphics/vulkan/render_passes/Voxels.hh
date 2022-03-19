#pragma once

#include "Common.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace sp::vulkan::renderer {
    class Lighting;

    class Voxels {
    public:
        Voxels(GPUScene &scene) : scene(scene) {}
        void LoadState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::VoxelArea, ecs::TransformSnapshot>> lock);

        void AddVoxelization(RenderGraph &graph, const Lighting &lighting);
        void AddDebugPass(RenderGraph &graph);

    private:
        GPUScene &scene;

        std::vector<ResourceID> fragmentListBuffers;

        ecs::Transform voxelToWorld;
        glm::ivec3 voxelGridSize;
    };
} // namespace sp::vulkan::renderer
