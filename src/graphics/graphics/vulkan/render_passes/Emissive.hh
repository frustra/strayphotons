/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Common.hh"
#include "ecs/Ecs.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace sp::vulkan::renderer {
    class Emissive {
    public:
        Emissive(GPUScene &scene) : scene(scene) {}
        void AddPass(RenderGraph &graph,
            ecs::Lock<ecs::Read<ecs::Screen, ecs::Gui, ecs::LaserLine, ecs::TransformSnapshot>> lock,
            chrono_clock::duration elapsedTime);

    private:
        GPUScene &scene;

        struct Screen {
            std::variant<ResourceID, TextureIndex> texture;

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
