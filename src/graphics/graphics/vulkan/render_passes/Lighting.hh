#pragma once

#include "Common.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <span>

namespace sp::vulkan::renderer {
    class Lighting {
    public:
        Lighting(GPUScene &scene) : scene(scene) {}
        void LoadState(RenderGraph &graph,
            ecs::Lock<ecs::Read<ecs::Name, ecs::Light, ecs::OpticalElement, ecs::TransformSnapshot>> lock);

        void AddShadowPasses(RenderGraph &graph);
        void AddGelTextures(RenderGraph &graph);
        void AddLightingPass(RenderGraph &graph);

    private:
        GPUScene &scene;

        uint32_t lightCount;
        glm::ivec2 shadowAtlasSize = {};
        ecs::View views[MAX_LIGHTS];
        std::vector<ecs::Entity> lightEntities;

        std::pair<string, const TextureIndex *> gelTextures[MAX_LIGHTS];
        robin_hood::unordered_map<string, TextureIndex> gelTextureCache;

        std::vector<InlineVector<uint32_t, MAX_LIGHTS>> lightPaths;
        std::vector<InlineVector<ecs::Entity, MAX_LIGHTS>> readbackLightPaths;

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
            glm::vec4 mapOffset;
            glm::vec2 clip;
            int gelId;
            float padding[1];
        };
        static_assert(sizeof(GPULight) == 17 * 4 * sizeof(float), "GPULight size incorrect");

        struct GPUData {
            GPULight lights[MAX_LIGHTS];
            int count;
            float padding[3];
        } gpuData;
    };
} // namespace sp::vulkan::renderer
