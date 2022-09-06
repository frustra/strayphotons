#pragma once

#include "Common.hh"

namespace sp::vulkan::renderer {
    class Emissive {
    public:
        void AddPass(RenderGraph &graph,
            ecs::Lock<ecs::Read<ecs::Screen, ecs::Gui, ecs::LaserLine, ecs::TransformSnapshot>> lock,
            chrono_clock::duration elapsedTime);

    private:
        struct Screen {
            ResourceID id;

            struct {
                glm::mat4 quad;
                glm::vec3 luminanceScale;
            } gpuData;
        };
        vector<Screen> screens;

        struct LaserLine {
            color_t color;
            float radius, mediaDensityFactor;
            glm::vec3 start, end;
        };
        vector<LaserLine> lasers;
    };
} // namespace sp::vulkan::renderer
