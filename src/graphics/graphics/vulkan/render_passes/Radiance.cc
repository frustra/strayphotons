/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Radiance.hh"

#include "console/CVar.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/render_passes/Voxels.hh"

namespace sp::vulkan::renderer {
    static CVar<bool> CVarEnableFlatlandRC("r.EnableFlatlandRC", true, "Enable 2D radiance cascade demo");
    static CVar<int> CVarRCDebug("r.RCDebug", 0, "Enable radiance cascade debug view (0: off, 1: voxel value)");
    static CVar<float> CVarRCDebugZoom("r.RCDebugZoom", 1, "Zoom factor for the radiance cascade debug view");
    static CVar<float> CVarRCDebugBlend("r.RCDebugBlend",
        0.0f,
        "The blend weight used to overlay radiance cascade debug");
    static CVar<int> CVarRCVoxelScale("r.RCVoxelScale", 4, "Number of probes along the length of a voxel");
    static CVar<int> CVarRCBaseSamples("r.RCBaseSamples", 8, "Number of samples per probe in cascade 0");
    static CVar<int> CVarRCNextSamples("r.RCNextSamples", 3, "Multiplier for number of probes per layer");
    static CVar<int> CVarRCNumCascades("r.RCNumCascades", 2, "Number of radiance cascades");
    static CVar<float> CVarRCTraceLength("r.RCTraceLength", 8, "Cascade trace length");

    void Radiance::AddFlatlandRC(RenderGraph &graph) {
        ZoneScoped;
        auto scope = graph.Scope("FlatlandRC");

        if (glm::any(glm::lessThanEqual(voxels.GetGridSize(), glm::uvec3(0)))) return;

        if (CVarEnableFlatlandRC.Get()) {
            for (int cascadeNum = 0; cascadeNum < CVarRCNumCascades.Get(); cascadeNum++) {
                std::string cascadeName = "RC-" + std::to_string(cascadeNum);
                int numSamples = CVarRCBaseSamples.Get() +
                                 CVarRCBaseSamples.Get() * cascadeNum * CVarRCNextSamples.Get();
                ImageDesc desc;
                graph.AddPass(cascadeName)
                    .Build([&](rg::PassBuilder &builder) {
                        builder.Read("Voxels.Radiance", Access::ComputeShaderSampleImage);
                        builder.Read("Voxels.Normals", Access::ComputeShaderSampleImage);
                        builder.ReadUniform("ViewState");
                        builder.ReadUniform("VoxelState");

                        auto gridSize = voxels.GetGridSize();
                        desc.extent = vk::Extent3D(gridSize.x * CVarRCVoxelScale.Get(),
                            gridSize.z * CVarRCVoxelScale.Get(),
                            1);
                        desc.arrayLayers = numSamples / std::pow(4, cascadeNum);
                        desc.primaryViewType = vk::ImageViewType::e2DArray;
                        desc.imageType = vk::ImageType::e2D;
                        desc.format = vk::Format::eR16G16B16A16Sfloat;
                        builder.CreateImage(cascadeName, desc, Access::ComputeShaderWrite);
                    })
                    .Execute([cascadeNum, cascadeName, gridSize = desc.extent, numSamples](rg::Resources &resources,
                                 CommandContext &cmd) {
                        cmd.SetComputeShader("rc_probes.comp");

                        cmd.SetUniformBuffer("ViewStates", "ViewState");
                        cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");
                        cmd.SetImageView("voxelRadiance", "Voxels.Radiance");
                        cmd.SetImageView("voxelNormals", "Voxels.Normals");
                        cmd.SetImageView("radianceOut", cascadeName);

                        cmd.SetShaderConstant(ShaderStage::Compute, "CASCADE_NUM", cascadeNum);
                        cmd.SetShaderConstant(ShaderStage::Compute, "NUM_SAMPLES", numSamples);
                        cmd.SetShaderConstant(ShaderStage::Compute, "SAMPLE_LENGTH", CVarRCTraceLength.Get());

                        cmd.Dispatch(gridSize.width >> cascadeNum, gridSize.height >> cascadeNum, numSamples);
                    });
            }
        }

        if (CVarRCDebug.Get() > 0) {
            graph.AddPass("RCDebug")
                .Build([&](rg::PassBuilder &builder) {
                    builder.Read("Voxels.Radiance", Access::FragmentShaderSampleImage);
                    builder.Read("Voxels.Normals", Access::FragmentShaderSampleImage);
                    builder.ReadUniform("ViewState");
                    builder.ReadUniform("VoxelState");
                    builder.Read("ExposureState", Access::FragmentShaderReadStorage);

                    for (int cascadeNum = 0; cascadeNum < CVarRCNumCascades.Get(); cascadeNum++) {
                        std::string cascadeName = "RC-" + std::to_string(cascadeNum);
                        builder.Read(cascadeName, Access::FragmentShaderSampleImage);
                    }

                    builder.Read(builder.LastOutputID(), Access::FragmentShaderSampleImage);

                    auto desc = builder.DeriveImage(builder.LastOutputID());
                    builder.OutputColorAttachment(0, "RCDebug", desc, {LoadOp::DontCare, StoreOp::Store});
                })
                .Execute([](rg::Resources &resources, CommandContext &cmd) {
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

                    for (int cascadeNum = 0; cascadeNum < CVarRCNumCascades.Get(); cascadeNum++) {
                        std::string cascadeName = "RC-" + std::to_string(cascadeNum);
                        auto imagePtr = resources.GetImageView(cascadeName);
                        cmd.SetImageView(1, cascadeNum, imagePtr);
                    }

                    cmd.SetShaderConstant(ShaderStage::Fragment, "DEBUG_MODE", CVarRCDebug.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "BLEND_WEIGHT", CVarRCDebugBlend.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "ZOOM", CVarRCDebugZoom.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "BASE_SAMPLES", CVarRCBaseSamples.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "NEXT_SAMPLES", CVarRCNextSamples.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "SAMPLE_LENGTH", CVarRCTraceLength.Get());

                    cmd.Draw(3);
                });
        }
    }
} // namespace sp::vulkan::renderer
