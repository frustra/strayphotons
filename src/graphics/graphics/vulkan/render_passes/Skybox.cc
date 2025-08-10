/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Skybox.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/components/View.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"
#include "graphics/vulkan/render_passes/Lighting.hh"

#include <glm/glm.hpp>

namespace sp::vulkan::renderer {
    static CVar<bool> CVarDrawSkybox0("r.DrawSkybox0", true, "Enable drawing the first skybox");
    static CVar<bool> CVarDrawSkybox1("r.DrawSkybox1", true, "Enable drawing the first skybox");
    static CVar<bool> CVarDrawSkybox2("r.DrawSkybox2", true, "Enable drawing the first skybox");
    static CVar<float> CVarSkyboxStarBrightness("r.SkyboxStarBrightness",
        0.08f,
        "Brightness scaling value for star skybox");
    static CVar<float> SkyboxStarDensity("r.SkyboxStarDensity", 100.0f, "Star tile density for skybox rendering");
    static CVar<float> CVarSkyboxStarSize("r.SkyboxStarSize", 0.0001f, "Star size for skybox rendering");

    void AddSkyboxPass(RenderGraph &graph) {
        static const std::array<glm::vec4, 3> rotations = {
            glm::vec4(1.0, 0.0, 0.0, M_2_PI),
            glm::vec4(glm::normalize(glm::vec3(1.0, 0.0, 1.0)), M_PI_2),
            glm::vec4(0.0, 0.0, 1.0, M_2_PI),
        };
        std::array<bool, 3> enabled = {
            CVarDrawSkybox0.Get(),
            CVarDrawSkybox1.Get(),
            CVarDrawSkybox2.Get(),
        };
        bool first = true;
        float brightness = CVarSkyboxStarBrightness.Get();
        float density = SkyboxStarDensity.Get();
        float starSize = CVarSkyboxStarSize.Get();
        for (size_t i = 0; i < rotations.size(); i++) {
            if (!enabled[i]) continue;
            graph.AddPass("Skybox" + std::to_string(i))
                .Build([&](rg::PassBuilder &builder) {
                    builder.Read("ExposureState", Access::FragmentShaderReadStorage);
                    builder.ReadUniform("ViewState");

                    builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
                    builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::ReadOnly});
                })
                .Execute([i, first, brightness, density, starSize](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetShaders("screen_cover.vert", "skybox.frag");

                    cmd.SetShaderConstant(ShaderStage::Vertex, "DRAW_DEPTH", 1.0f);

                    if (!first) {
                        cmd.SetBlending(true, vk::BlendOp::eMax);
                        cmd.SetBlendFunc(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOne);
                    }

                    cmd.SetDepthTest(true, false);
                    cmd.SetDepthCompareOp(vk::CompareOp::eEqual);

                    cmd.SetStorageBuffer("ExposureState", "ExposureState");
                    cmd.SetUniformBuffer("ViewStates", "ViewState");

                    struct {
                        glm::mat4 rotation;
                        uint32_t index;
                        float brightness, density, starSize;
                    } constants = {
                        glm::mat4(glm::angleAxis(rotations[i].w, glm::vec3(rotations[i]))),
                        (uint32_t)i,
                        brightness,
                        density,
                        starSize,
                    };

                    cmd.PushConstants(constants);

                    cmd.Draw(3);
                });
            first = false;
        }
    }
} // namespace sp::vulkan::renderer
