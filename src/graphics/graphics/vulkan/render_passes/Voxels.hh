/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"
#include "console/CFunc.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <cstdint>

namespace sp::vulkan::renderer {
    static const uint32_t MAX_VOXEL_FRAGMENT_LISTS = 16;

    extern CVar<bool> CVarEnableVoxels;
    extern CVar<bool> CVarEnableVoxels2;
    extern CVar<int> CVarVoxelDebug;
    extern CVar<float> CVarVoxelDebugBlend;
    extern CVar<uint32_t> CVarVoxelDebugMip;
    extern CVar<size_t> CVarVoxelLayers;
    extern CVar<int> CVarVoxelClear;
    extern CVar<float> CVarLightAttenuation;
    extern CVar<float> CVarLightLowPass;
    extern CVar<uint32_t> CVarVoxelFillIndex;
    extern CVar<bool> CVarReprojectVoxelGrid;
    extern CVar<uint32_t> CVarVoxelFragmentBuckets;
    extern CVar<float> CVarVoxelFragmentBucketSizeFactor;

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

        uint32_t GetLayerCount() const {
            return voxelLayerCount;
        }

        glm::ivec3 GetGridSize() const {
            return voxelGridSize;
        }

    private:
        GPUScene &scene;

        struct FragmentListSize {
            uint32_t capacity, offset;
        };
        std::array<FragmentListSize, MAX_VOXEL_FRAGMENT_LISTS> fragmentListSizes;
        uint32_t fragmentListCount;

        ecs::Transform voxelToWorld;
        glm::ivec3 voxelGridSize = glm::ivec3(0);
        uint32_t voxelLayerCount;

        std::atomic_flag debugThisFrame;
        CFuncCollection funcs;

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
                        "Voxels2/" + name,
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
