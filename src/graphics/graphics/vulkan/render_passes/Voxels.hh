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

        ecs::Transform voxelGridOrigin;
        glm::ivec3 voxelGridSize = glm::ivec3(1);

        struct GPUData {
            glm::mat4 origin;
            glm::ivec3 size;
        } gpuData;
    };
} // namespace sp::vulkan::renderer
