#pragma once

#include "Common.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace sp::vulkan::renderer {
    static const uint32 MAX_VOXEL_FRAGMENT_LISTS = 16;

    class Lighting;

    class Voxels {
    public:
        Voxels(GPUScene &scene) : scene(scene) {}
        void LoadState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::VoxelArea, ecs::TransformSnapshot>> lock);

        void AddVoxelization(RenderGraph &graph, const Lighting &lighting);
        void AddVoxelization2(RenderGraph &graph, const Lighting &lighting);
        void AddDebugPass(RenderGraph &graph);

    private:
        GPUScene &scene;

        struct FragmentListSize {
            uint32 capacity, offset;
        };
        std::array<FragmentListSize, MAX_VOXEL_FRAGMENT_LISTS> fragmentListSizes;
        uint32 fragmentListCount;

        ecs::Transform voxelToWorld;
        glm::ivec3 voxelGridSize;
    };
} // namespace sp::vulkan::renderer
