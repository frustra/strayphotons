/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"
#include "console/CVar.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <optional>

namespace sp::vulkan::renderer {
    static const int MAX_LIGHTS = 64;
    static const int MAX_OPTICS = 16;

    extern CVar<int> CVarShadowMapSampleCount;
    extern CVar<float> CVarShadowMapSampleWidth;

    class Voxels;

    class Lighting {
    public:
        Lighting(GPUScene &scene, Voxels &voxels) : scene(scene), voxels(voxels) {}
        void LoadState(RenderGraph &graph,
            ecs::Lock<ecs::Read<ecs::Light, ecs::OpticalElement, ecs::TransformSnapshot>> lock);

        void AddShadowPasses(RenderGraph &graph);
        void AddGelTextures(RenderGraph &graph);
        void AddLightingPass(RenderGraph &graph);
        void AddVolumetricShadows(RenderGraph &graph);

        bool PreloadGelTextures(ecs::Lock<ecs::Read<ecs::Light>> lock);

    private:
        void AllocateShadowMap();
        GPUScene &scene;
        Voxels &voxels;

        glm::ivec2 shadowAtlasSize = {};

        struct LightPathEntry {
            ecs::Entity ent;
            bool reflect;

            LightPathEntry() {}
            LightPathEntry(ecs::Entity ent, bool reflect) : ent(ent), reflect(reflect) {}

            bool operator==(const LightPathEntry &) const = default;
            bool operator==(const GPUScene::OpticInstance &other) const {
                return ent == other.ent;
            }
        };

        struct VirtualLight {
            ecs::Entity source;
            InlineVector<LightPathEntry, MAX_LIGHTS> lightPath; // A Light followed by N OpticalElement's
            std::optional<uint32_t> parentIndex;
            std::optional<uint32_t> opticIndex;

            std::string gelName;
            std::optional<TextureIndex> gelTexture;

            bool operator==(const VirtualLight &) const;
        };

        robin_hood::unordered_map<string, TextureHandle> gelTextureCache;

        std::array<ecs::View, MAX_LIGHTS> views;
        std::vector<VirtualLight> lights; // Current frame shadowmap lights
        std::vector<VirtualLight> previousLights; // Previous frame shadowmap lights
        std::vector<VirtualLight> readbackLights; // Optic visibility readback
        std::vector<std::pair<glm::ivec2, glm::ivec2>> freeRectangles;

        struct GPULight {
            glm::vec3 position;
            float spotAngleCos;

            glm::vec3 tint;
            float intensity;

            glm::vec3 direction;
            float illuminance;

            glm::mat4 proj;
            glm::mat4 invProj;
            glm::mat4 view;
            glm::mat4 invView;
            glm::vec4 mapOffset;
            glm::vec4 bounds;
            std::array<glm::vec2, 4> cornerUVs; // clockwise winding starting at the bottom left
            glm::vec2 clip;
            uint32_t gelId;
            uint32_t previousIndex;
            uint32_t parentIndex;

            float padding[3];
        };
        static_assert(sizeof(GPULight) == 25 * 4 * sizeof(float), "GPULight size incorrect");

        struct GPUData {
            GPULight lights[MAX_LIGHTS];
            int count;
            float padding[3];
        } gpuData;
    };
} // namespace sp::vulkan::renderer
