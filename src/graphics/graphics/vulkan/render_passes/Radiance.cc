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
    static CVar<float> CVarRCDebugZoom("r.RCDebugZoom", 0.5, "Zoom factor for the radiance cascade debug view");
    static CVar<float> CVarRCDebugBlend("r.RCDebugBlend",
        0.0f,
        "The blend weight used to overlay radiance cascade debug");
    static CVar<float> CVarRCVoxelScale("r.RCVoxelScale", 1, "Number of probes along the length of a voxel");
    static CVar<int> CVarRCBaseSamples("r.RCBaseSamples", 8, "Number of samples per probe in cascade 0");
    static CVar<int> CVarRCNextSamples("r.RCNextSamples", 3, "Multiplier for number of probes per layer");
    static CVar<int> CVarRCNumCascades("r.RCNumCascades", 5, "Number of radiance cascades");
    static CVar<float> CVarRCTraceLength("r.RCTraceLength", 2, "Cascade trace length");
    static CVar<int> CVarRCResolution("r.RCResolution", 512, "Radiance Cascade evaluation resolution");

    void Radiance::AddFlatlandRC(RenderGraph &graph) {
        ZoneScoped;
        auto scope = graph.Scope("FlatlandRC");

        const glm::uvec3 voxelGridSize = voxels.GetGridSize();
        if (glm::any(glm::lessThanEqual(voxelGridSize, glm::uvec3(0)))) return;

        const glm::uvec2 resolution = glm::uvec2(CVarRCResolution.Get(),
            CVarRCResolution.Get() * voxelGridSize.z / voxelGridSize.x);
        const int numCascades = CVarRCNumCascades.Get();
        const int baseSamples = CVarRCBaseSamples.Get();
        const int nextSamples = CVarRCNextSamples.Get();

        if (CVarEnableFlatlandRC.Get()) {
            for (int cascadeNum = numCascades - 1; cascadeNum >= 0; cascadeNum--) {
                std::string cascadeName = "RC" + std::to_string(cascadeNum);
                int numSamples = baseSamples * std::pow(nextSamples, cascadeNum);
                ImageDesc desc;
                graph.AddPass(cascadeName)
                    .Build([&](rg::PassBuilder &builder) {
                        builder.Read("Voxels.Radiance", Access::ComputeShaderSampleImage);
                        builder.Read("Voxels.Normals", Access::ComputeShaderSampleImage);
                        builder.ReadUniform("ViewState");
                        builder.ReadUniform("VoxelState");

                        desc.extent = vk::Extent3D(resolution.x / (1 << cascadeNum),
                            resolution.y / (1 << cascadeNum),
                            1);
                        desc.arrayLayers = numSamples; // / std::pow(2, cascadeNum + 1);
                        desc.primaryViewType = vk::ImageViewType::e2DArray;
                        desc.imageType = vk::ImageType::e2D;
                        desc.format = vk::Format::eR16G16B16A16Sfloat;
                        builder.CreateImage(cascadeName, desc, Access::ComputeShaderWrite);
                    })
                    .Execute([cascadeNum,
                                 cascadeName,
                                 resolution,
                                 gridSize = glm::ivec3(desc.extent.width, desc.extent.height, desc.arrayLayers),
                                 baseSamples,
                                 nextSamples](rg::Resources &resources, CommandContext &cmd) {
                        cmd.SetComputeShader("rc_probes.comp");

                        cmd.SetUniformBuffer("ViewStates", "ViewState");
                        cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");
                        cmd.SetImageView("voxelRadiance", "Voxels.Radiance");
                        cmd.SetImageView("voxelNormals", "Voxels.Normals");
                        cmd.SetImageView("radianceOut", cascadeName);

                        cmd.SetShaderConstant(ShaderStage::Compute, "CASCADE_NUM", cascadeNum);
                        cmd.SetShaderConstant(ShaderStage::Compute, "BASE_SAMPLES", baseSamples);
                        cmd.SetShaderConstant(ShaderStage::Compute, "NEXT_SAMPLES", nextSamples);
                        cmd.SetShaderConstant(ShaderStage::Compute, "SAMPLE_LENGTH", CVarRCTraceLength.Get());
                        cmd.SetShaderConstant(ShaderStage::Compute, "VOXEL_SCALE", CVarRCVoxelScale.Get());
                        cmd.SetShaderConstant(ShaderStage::Compute, "RS_RESOLUTION_X", resolution.x);
                        cmd.SetShaderConstant(ShaderStage::Compute, "RS_RESOLUTION_Y", resolution.y);

                        cmd.Dispatch(gridSize.x, gridSize.y, gridSize.z);
                    });

                if (cascadeNum + 1 >= numCascades) continue;

                std::string cascadeIn1Name = "RCMerged" + std::to_string(cascadeNum + 1);
                if (cascadeNum + 2 >= numCascades) cascadeIn1Name = "RC" + std::to_string(cascadeNum + 1);
                std::string cascadeOutName = "RCMerged" + std::to_string(cascadeNum);
                graph.AddPass(cascadeOutName)
                    .Build([&](rg::PassBuilder &builder) {
                        builder.Read(cascadeName, Access::ComputeShaderReadStorage);
                        builder.Read(cascadeIn1Name, Access::ComputeShaderReadStorage);

                        builder.CreateImage(cascadeOutName, desc, Access::ComputeShaderWrite);
                    })
                    .Execute([cascadeNum,
                                 cascadeName,
                                 cascadeIn1Name,
                                 cascadeOutName,
                                 gridSize = glm::ivec3(desc.extent.width, desc.extent.height, desc.arrayLayers),
                                 baseSamples,
                                 nextSamples](rg::Resources &resources, CommandContext &cmd) {
                        cmd.SetComputeShader("rc_merge.comp");

                        cmd.SetImageView("radianceIn0", cascadeName);
                        cmd.SetImageView("radianceIn1", cascadeIn1Name);
                        cmd.SetImageView("radianceOut", cascadeOutName);

                        cmd.SetShaderConstant(ShaderStage::Compute, "CASCADE_NUM", cascadeNum);
                        cmd.SetShaderConstant(ShaderStage::Compute, "BASE_SAMPLES", baseSamples);
                        cmd.SetShaderConstant(ShaderStage::Compute, "NEXT_SAMPLES", nextSamples);
                        cmd.SetShaderConstant(ShaderStage::Compute, "SAMPLE_LENGTH", CVarRCTraceLength.Get());
                        cmd.SetShaderConstant(ShaderStage::Compute, "VOXEL_SCALE", CVarRCVoxelScale.Get());

                        cmd.Dispatch(gridSize.x, gridSize.y, gridSize.z);
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

                    for (int cascadeNum = 0; cascadeNum < numCascades; cascadeNum++) {
                        std::string cascadeName = "RC" + std::to_string(cascadeNum);
                        builder.Read(cascadeName, Access::FragmentShaderSampleImage);
                        if (cascadeNum + 1 < numCascades) {
                            cascadeName = "RCMerged" + std::to_string(cascadeNum);
                            builder.Read(cascadeName, Access::FragmentShaderSampleImage);
                        }
                    }

                    builder.Read(builder.LastOutputID(), Access::FragmentShaderSampleImage);

                    auto desc = builder.DeriveImage(builder.LastOutputID());
                    builder.OutputColorAttachment(0, "RCDebug", desc, {LoadOp::DontCare, StoreOp::Store});
                })
                .Execute([numCascades, baseSamples, nextSamples](rg::Resources &resources, CommandContext &cmd) {
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

                    for (int i = 0; i < numCascades; i++) {
                        cmd.SetImageView(1, i, resources.GetImageView("RC" + std::to_string(i)));
                        if (i + 1 < numCascades) {
                            cmd.SetImageView(2, i, resources.GetImageView("RCMerged" + std::to_string(i)));
                        } else if (i == 0) {
                            cmd.SetImageView(2, 0, resources.GetImageView("RC0"));
                        }
                    }

                    cmd.SetShaderConstant(ShaderStage::Fragment, "DEBUG_MODE", CVarRCDebug.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "BLEND_WEIGHT", CVarRCDebugBlend.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "ZOOM", CVarRCDebugZoom.Get());
                    cmd.SetShaderConstant(ShaderStage::Fragment, "BASE_SAMPLES", baseSamples);
                    cmd.SetShaderConstant(ShaderStage::Fragment, "NEXT_SAMPLES", nextSamples);
                    cmd.SetShaderConstant(ShaderStage::Fragment, "SAMPLE_LENGTH", CVarRCTraceLength.Get());

                    cmd.Draw(3);
                });
        }
    }
} // namespace sp::vulkan::renderer
