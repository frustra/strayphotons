#pragma once

#include "Common.hh"

namespace sp::vulkan::renderer {
    class Emissive {
    public:
        void AddPass(RenderGraph &graph,
            ecs::Lock<ecs::Read<ecs::Screen, ecs::LaserLine, ecs::TransformSnapshot>> lock);

    private:
        struct Screen {
            ResourceID id;

            struct {
                glm::mat4 quad;
                glm::vec3 luminanceScale;
            } gpuData;
        };
        vector<Screen> screens;

        struct LaserContext {
            struct GPULine {
                glm::vec3 color;
                float padding0[1];
                glm::vec3 start;
                float padding1[1];
                glm::vec3 end;
                float padding2[1];
            };
            static_assert(sizeof(GPULine) % sizeof(glm::vec4) == 0, "std430 alignment");

            vector<GPULine> gpuData;
        };
        LaserContext lasers;
    };
} // namespace sp::vulkan::renderer
