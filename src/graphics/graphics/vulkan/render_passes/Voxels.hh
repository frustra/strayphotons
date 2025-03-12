/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"
#include "console/CFunc.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace sp::vulkan::renderer {
    static const uint32 MAX_VOXEL_FRAGMENT_LISTS = 16;

    class Lighting;

    struct VoxelLayerInfo {
        std::string name, fullName, preBlurName;
        uint32_t layerIndex;
        uint32_t dirIndex;
    };

    class Voxels {
    public:
        Voxels(GPUScene &scene);
        void LoadState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::VoxelArea, ecs::TransformSnapshot>> lock);

        void AddVoxelization(RenderGraph &graph, const Lighting &lighting);
        void AddVoxelizationInit(RenderGraph &graph, const Lighting &lighting);
        void AddVoxelization2(RenderGraph &graph, const Lighting &lighting);
        void AddDebugPass(RenderGraph &graph);

        vk::DescriptorSet GetCurrentVoxelDescriptorSet() const;

        uint32_t GetLayerCount() const {
            return voxelLayerCount;
        }

    private:
        GPUScene &scene;

        struct FragmentListSize {
            uint32 capacity, offset;
        };
        std::array<FragmentListSize, MAX_VOXEL_FRAGMENT_LISTS> fragmentListSizes;
        uint32 fragmentListCount;

        ecs::Transform voxelToWorld;
        glm::ivec3 voxelGridSize = glm::ivec3(0);
        uint32_t voxelLayerCount;

        std::array<vk::DescriptorSet, 2> layerDescriptorSets;
        uint32_t currentSetFrame = 0;

        std::atomic_flag debugThisFrame;
        CFuncCollection funcs;

        void updateDescriptorSet(rg::Resources &resources, DeviceContext &device);

        static inline const std::array<glm::vec3, 6> directions = {
            glm::vec3(1, 0, 0),
            glm::vec3(0, 1, 0),
            glm::vec3(0, 0, 1),
            glm::vec3(-1, 0, 0),
            glm::vec3(0, -1, 0),
            glm::vec3(0, 0, -1),
        };
        using VoxelInfoIndex = std::array<std::array<VoxelLayerInfo, directions.size()>, MAX_VOXEL_FRAGMENT_LISTS>;

        static inline VoxelInfoIndex generateVoxelLayerInfo() {
            VoxelInfoIndex layers;
            for (uint32_t i = 0; i < MAX_VOXEL_FRAGMENT_LISTS; i++) {
                for (uint32_t dir = 0; dir < directions.size(); dir++) {
                    std::string name = "VoxelLayer" + std::to_string(i) + "_" + std::to_string(dir);
                    layers[i][dir] = VoxelLayerInfo{
                        name,
                        "Voxels2." + name,
                        name + "_PreBlur",
                        i,
                        dir,
                    };
                }
            }
            return layers;
        }

    public:
        static inline const auto VoxelLayers = generateVoxelLayerInfo();
    };
} // namespace sp::vulkan::renderer
