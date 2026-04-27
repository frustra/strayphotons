/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Tonemap.hh"

#include "console/CVar.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/Shader.hh"

namespace sp::vulkan::renderer {
    static CVar<glm::vec3> CVarWhitePoint("r.TonemapWhitePoint",
        glm::vec3(8),
        "The target whitepoint for HDR to SDR tonemapping.");
    static CVar<float> CVarGammaCurve("r.TonemapGammaCurve", 2.9, "The gamma curve scale for your display.");
    static CVar<float> CVarDarkSaturation("r.TonemapDarkSaturation",
        0.7,
        "The HSV saturation scale for dark parts of the scene.");
    static CVar<float> CVarBrightSaturation("r.TonemapBrightSaturation",
        1.1,
        "The HSV saturation scale for bright parts of the scene.");

    void AddTonemap(RenderGraph &graph) {
        glm::vec3 whitePoint = CVarWhitePoint.Get();
        float gammaCurve = CVarGammaCurve.Get();
        float darkSaturation = CVarDarkSaturation.Get();
        float brightSaturation = CVarBrightSaturation.Get();
        graph.AddPass("Tonemap")
            .Build([&](rg::PassBuilder &builder) {
                auto luminanceID = builder.LastOutputID();
                builder.Read(luminanceID, Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveImage(luminanceID);
                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "TonemappedLuminance", desc, {LoadOp::DontCare, StoreOp::Store});
            })
            .Execute([whitePoint, gammaCurve, darkSaturation, brightSaturation](rg::Resources &resources,
                         CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "tonemap.frag");
                cmd.SetImageView("luminanceTex", resources.LastOutputID());
                cmd.SetShaderConstant(ShaderStage::Fragment, "WHITE_POINT_R", whitePoint.r);
                cmd.SetShaderConstant(ShaderStage::Fragment, "WHITE_POINT_G", whitePoint.g);
                cmd.SetShaderConstant(ShaderStage::Fragment, "WHITE_POINT_B", whitePoint.b);

                cmd.SetShaderConstant(ShaderStage::Fragment, "CURVE_SCALE", gammaCurve);
                cmd.SetShaderConstant(ShaderStage::Fragment, "DARK_SATURATION", darkSaturation);
                cmd.SetShaderConstant(ShaderStage::Fragment, "BRIGHT_SATURATION", brightSaturation);

                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
