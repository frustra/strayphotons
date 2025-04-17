/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Tonemap.hh"
#include "console/CVar.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/render_passes/Voxels.hh"

namespace sp::vulkan::renderer {
    static CVar<bool> CVarEnableFlatlandRC("r.EnableFlatlandRC", true, "Enable 2D radiance cascade demo");
    static CVar<int> CVarRCDebug("r.RCDebug", 0, "Enable radiance cascade debug view (0: off, 1: voxel value)");
    static CVar<size_t> CVarRCDebugLayer("r.RCDebugLayer", 0, "Layer to debug for radiance cascades");
    static CVar<float> CVarRCDebugBlend("r.RCDebugBlend",
        0.0f,
        "The blend weight used to overlay radiance cascade debug");

    void AddFlatlandRC(RenderGraph &graph) {
        ZoneScoped;
        auto scope = graph.Scope("FlatlandRC");

        if (CVarRCDebug.Get() > 0) {
            int debugLayer = std::min(CVarRCDebugLayer.Get(), Voxels::VoxelLayers.size() - 1);

            graph.AddPass("RCDebug")
                .Build([&](rg::PassBuilder &builder) {
                    builder.Read("Voxels.Radiance", Access::FragmentShaderSampleImage);
                    builder.Read("Voxels.Normals", Access::FragmentShaderSampleImage);
                    builder.ReadUniform("ViewState");
                    builder.ReadUniform("VoxelState");
                    builder.Read("ExposureState", Access::FragmentShaderReadStorage);

                    builder.Read(builder.LastOutputID(), Access::FragmentShaderSampleImage);

                    auto desc = builder.DeriveImage(builder.LastOutputID());
                    builder.OutputColorAttachment(0, "RCDebug", desc, {LoadOp::DontCare, StoreOp::Store});
                })
                .Execute([debugLayer](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetShaders("screen_cover.vert", "rc_debug.frag");
                    cmd.SetStencilTest(true);
                    cmd.SetDepthTest(false, false);
                    cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                    cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                    cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                    cmd.SetUniformBuffer("ViewStates", "ViewState");
                    cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");
                    cmd.SetStorageBuffer("ExposureState", "ExposureState");
                    cmd.SetImageView("overlayTex", resources.LastOutputID());
                    cmd.SetImageView("voxelRadiance", "Voxels.Radiance");
                    cmd.SetImageView("voxelNormals", "Voxels.Normals");

                    cmd.SetShaderConstant(ShaderStage::Fragment, "DEBUG_MODE", CVarRCDebug.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "BLEND_WEIGHT", CVarRCDebugBlend.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "VOXEL_LAYER", debugLayer);

                    cmd.Draw(3);
                });
        }
    }
} // namespace sp::vulkan::renderer
