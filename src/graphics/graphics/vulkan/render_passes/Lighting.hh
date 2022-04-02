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

        glm::ivec2 shadowAtlasSize = {};

        struct VirtualLight {
            ecs::View view;
            ecs::Entity source; // Real light or last optic entity in source light path
            uint32_t sourceIndex; // Virtual light index of source
            uint32_t opticIndex = ~(uint32_t)0; // Optic index of output optic

            std::string gelName;
            const TextureIndex *gelTexture = nullptr;
        };

        robin_hood::unordered_map<string, TextureIndex> gelTextureCache;

        std::vector<VirtualLight> lights;
        std::vector<VirtualLight> readbackLights;

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
            glm::vec4 bounds;
            glm::vec2 clip;
            int gelId;
            float padding[1];
        };
        static_assert(sizeof(GPULight) == 18 * 4 * sizeof(float), "GPULight size incorrect");

        struct GPUData {
            GPULight lights[MAX_LIGHTS];
            int count;
            float padding[3];
        } gpuData;
    };
} // namespace sp::vulkan::renderer
