/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Voxels.hh"

#include "MarchingCubes.h"
#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/Shader.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/PassBuilder.hh"
#include "graphics/vulkan/render_passes/Lighting.hh"
#include "graphics/vulkan/render_passes/Readback.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace sp::vulkan::renderer {
    static CVar<bool> CVarEnableVoxels("r.EnableVoxels", true, "Enable world voxelization for lighting");
    static CVar<bool> CVarEnableVoxels2("r.EnableVoxels2", true, "Enable world voxelization for lighting");
    static CVar<uint32_t> CVarEnableMarchingCubes("r.EnableMarchingCubes",
        0,
        "Enable converting the voxel grid into a triangle mesh (0: off, 1: basic, 2: advanced)");
    static CVar<uint32_t> CVarMarchingCubesLayer("r.MarchingCubesLayer",
        0,
        "The voxel grid layer to convert to a triangle mesh");
    static CVar<int> CVarVoxelDebug("r.VoxelDebug",
        0,
        "Enable voxel grid debug view (0: off, 1: ray march, 2: cone trace, 3: diffuse trace)");
    static CVar<float> CVarVoxelDebugBlend("r.VoxelDebugBlend", 0.0f, "The blend weight used to overlay voxel debug");
    static CVar<uint32_t> CVarVoxelDebugMip("r.VoxelDebugMip", 0, "The voxel mipmap to sample in the debug view");
    static CVar<size_t> CVarVoxelLayers("r.VoxelLayers", 3, "The number of voxel mipmap layers");
    static CVar<int> CVarVoxelClear("r.VoxelClear",
        15,
        "Change the voxel grid clearing operation used between frames "
        "(bitfield: 1=radiance, 2=counters, 4=normals, 8=mipmap)");
    static CVar<float> CVarLightAttenuation("r.LightAttenuation", 0.1f, "Light attenuation for voxel bounces");
    static CVar<float> CVarLightLowPass("r.LightLowPass",
        0.95,
        "Blend this amount of light in from the previous frame");
    static CVar<uint32_t> CVarVoxelFillIndex("r.VoxelFillIndex", 7, "Voxel layer index to read for light feedback");
    static CVar<bool> CVarReprojectVoxelGrid("r.VoxelReprojectGrid",
        true,
        "Account for the voxel grid moving when sampling previous frames");

    static CVar<uint32_t> CVarVoxelFragmentBuckets("r.VoxelFragmentBuckets",
        9,
        "The number of fragments that can be written to a voxel.");

    static CVar<float> CVarVoxelFragmentBucketSizeFactor("r.VoxelFragmentBucketSizeFactor",
        1.75f,
        "Factor to decrease size of subsequent buckets");

    struct GPUVoxelState {
        glm::mat4 worldToVoxel;
        glm::ivec3 size;
        float _padding[1];
    };

    struct GPUVoxelFragment {
        uint16_t position[3];
        uint16_t _padding0[1];
        float16_t radiance[3];
        uint16_t _padding1[1];
        float16_t normal[3];
        uint16_t _padding2[1];
    };

    struct GPUVoxelFragmentList {
        uint32_t count;
        uint32_t capacity;
        uint32_t offset;
        VkDispatchIndirectCommand cmd;
    };

    Voxels::Voxels(GPUScene &scene) : scene(scene) {
        funcs.Register("printgraphics", "Print graphics debug information", [this]() {
            if (debugThisFrame[0].test_and_set()) {
                Warnf("Graphics frame already flagged for debug printing");
            }
        });
        funcs.Register("printvoxels", "Print graphics debug information", [this]() {
            if (debugThisFrame[1].test_and_set()) {
                Warnf("Graphics frame already flagged for debug printing");
            }
        });
    }

    void Voxels::LoadState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::VoxelArea, ecs::TransformSnapshot>> lock) {
        voxelGridSize = glm::ivec3(0);
        for (const ecs::Entity &entity : lock.EntitiesWith<ecs::VoxelArea>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &area = entity.Get<ecs::VoxelArea>(lock);
            if (area.extents.x > 0 && area.extents.y > 0 && area.extents.z > 0) {
                voxelGridSize = area.extents;
                voxelToWorld = entity.Get<ecs::TransformSnapshot>(lock);
                break; // Only 1 voxel area supported for now
            }
        }

        // currentSetFrame = (currentSetFrame + 1) % layerDescriptorSets.size();
        // if (!layerDescriptorSets[currentSetFrame]) {
        //     layerDescriptorSets[currentSetFrame] = graph.Device().CreateBindlessDescriptorSet();
        // }

        graph.AddPass("VoxelState")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateUniform("VoxelState", sizeof(GPUVoxelState));
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                GPUVoxelState gpuData = {voxelToWorld.GetInverse().GetMatrix(), voxelGridSize};
                resources.GetBuffer("VoxelState")->CopyFrom(&gpuData);
            });
    }

    void Voxels::updateDescriptorSet(rg::Resources &resources, DeviceContext &device) {
        if (voxelLayerCount == 0) return;

        std::vector<vk::DescriptorImageInfo> descriptorImageInfos;
        descriptorImageInfos.reserve(voxelLayerCount * 6);

        for (size_t layer = 0; layer < voxelLayerCount; layer++) {
            for (auto &voxelLayer : VoxelLayers[layer]) {
                auto tex = resources.GetImageView(voxelLayer.name);
                descriptorImageInfos.emplace_back(tex->DefaultSampler(), *tex, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        }

        vk::WriteDescriptorSet descriptorWrite;
        descriptorWrite.dstSet = GetCurrentVoxelDescriptorSet();
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptorWrite.pImageInfo = descriptorImageInfos.data();
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorCount = descriptorImageInfos.size();

        device->updateDescriptorSets({descriptorWrite}, {});
    }

    void Voxels::AddVoxelization(RenderGraph &graph, const Lighting &lighting) {
        ZoneScoped;
        auto scope = graph.Scope("Voxels");

        if (voxelGridSize == glm::ivec3(0) || !CVarEnableVoxels.Get()) {
            graph.AddPass("Dummy")
                .Build([&](rg::PassBuilder &builder) {
                    ImageDesc desc;
                    desc.extent = vk::Extent3D(1, 1, 1);
                    desc.primaryViewType = vk::ImageViewType::e3D;
                    desc.imageType = vk::ImageType::e3D;
                    desc.format = vk::Format::eR16G16B16A16Sfloat;
                    builder.CreateImage("Radiance", desc, Access::TransferWrite);
                    // desc.format = vk::Format::eR8G8B8A8Snorm;
                    builder.CreateImage("Normals", desc, Access::TransferWrite);
                })
                .Execute([](rg::Resources &resources, CommandContext &cmd) {
                    auto radianceView = resources.GetImageView("Radiance");
                    auto normalsView = resources.GetImageView("Normals");

                    vk::ClearColorValue clear;
                    clear.setFloat32({0.0f, 0.0f, 0.0f, 1.0f});
                    vk::ImageSubresourceRange range;
                    range.layerCount = 1;
                    range.levelCount = 1;
                    range.aspectMask = vk::ImageAspectFlagBits::eColor;
                    cmd.Raw().clearColorImage(*radianceView->Image(),
                        vk::ImageLayout::eTransferDstOptimal,
                        clear,
                        {range});

                    cmd.Raw().clearColorImage(*normalsView->Image(),
                        vk::ImageLayout::eTransferDstOptimal,
                        clear,
                        {range});
                });
            return;
        }

        ecs::View ortho;
        ortho.visibilityMask = ecs::VisibilityMask::LightingVoxel;

        auto voxelCenter = voxelToWorld;
        voxelCenter.Translate(voxelCenter * (0.5f * glm::vec4(voxelGridSize, 0)));

        std::array<ecs::Transform, 3> axisTransform = {voxelCenter, voxelCenter, voxelCenter};
        axisTransform[0].RotateAxis(M_PI_2, glm::vec3(0, 1, 0));
        axisTransform[1].RotateAxis(M_PI_2, glm::vec3(1, 0, 0));

        axisTransform[0].Scale(glm::vec3(voxelGridSize.z, voxelGridSize.y, voxelGridSize.x));
        axisTransform[1].Scale(glm::vec3(voxelGridSize.x, voxelGridSize.z, voxelGridSize.y));
        axisTransform[2].Scale(voxelGridSize);

        std::array<ecs::View, 3> orthoAxes = {ortho, ortho, ortho};
        orthoAxes[0].extents = glm::ivec2(voxelGridSize.z, voxelGridSize.y);
        orthoAxes[1].extents = glm::ivec2(voxelGridSize.x, voxelGridSize.z);
        orthoAxes[2].extents = glm::ivec2(voxelGridSize.x, voxelGridSize.y);
        for (size_t i = 0; i < orthoAxes.size(); i++) {
            auto axis = axisTransform[i];
            axis.Scale(glm::vec3(0.5, 0.5, 1));
            axis.Translate(axis * glm::vec4(0, 0, -0.5, 0));
            orthoAxes[i].SetInvViewMat(axis.GetMatrix());
        }

        bool clearRadiance = (CVarVoxelClear.Get() & 1) == 1 && CVarEnableSpecularTracing.Get();
        bool clearCounters = (CVarVoxelClear.Get() & 2) == 2;
        bool clearNormals = (CVarVoxelClear.Get() & 4) == 4 && CVarEnableSpecularTracing.Get();
        auto drawID = scene.GenerateDrawsForView(graph, ortho.visibilityMask, 3);
        if (!drawID) return;

        auto voxelGridExtents = vk::Extent3D(voxelGridSize.x, voxelGridSize.y, voxelGridSize.z);
        auto voxelGridMips = CalculateMipmapLevels(voxelGridExtents);

        fragmentListCount = std::min(MAX_VOXEL_FRAGMENT_LISTS, CVarVoxelFragmentBuckets.Get());
        auto voxelFillIndex = std::min(CVarVoxelFillIndex.Get(), voxelLayerCount - 1);

        uint32_t totalFragmentListSize = 0;
        {
            auto fragmentListSize = (voxelGridSize.x * voxelGridSize.y * voxelGridSize.z) / 2;
            for (uint32_t i = 0; i < fragmentListCount; i++) {
                fragmentListSizes[i].capacity = fragmentListSize;
                fragmentListSizes[i].offset = totalFragmentListSize;
                totalFragmentListSize += fragmentListSize;
                fragmentListSize /= CVarVoxelFragmentBucketSizeFactor.Get();
            }
        }

        graph.AddPass("Init")
            .Build([&](rg::PassBuilder &builder) {
                ImageDesc desc;
                desc.extent = voxelGridExtents;
                desc.primaryViewType = vk::ImageViewType::e3D;
                desc.imageType = vk::ImageType::e3D;
                desc.mipLevels = voxelGridMips;
                desc.sampler = SamplerType::TrilinearClampBorder;
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.CreateImage("Radiance", desc, clearRadiance ? Access::TransferWrite : Access::None);
                // desc.format = vk::Format::eR8G8B8A8Snorm;
                builder.CreateImage("Normals", desc, clearNormals ? Access::TransferWrite : Access::None);

                builder.CreateBuffer("FillCounters",
                    {sizeof(uint32_t), voxelGridExtents.width * voxelGridExtents.height * voxelGridExtents.depth},
                    Residency::GPU_ONLY,
                    clearCounters ? Access::TransferWrite : Access::None);

                builder.CreateBuffer("FragmentListMetadata",
                    {sizeof(GPUVoxelFragmentList), MAX_VOXEL_FRAGMENT_LISTS},
                    Residency::GPU_ONLY,
                    Access::TransferWrite);

                builder.CreateBuffer("FragmentLists",
                    {sizeof(GPUVoxelFragment), totalFragmentListSize},
                    Residency::GPU_ONLY,
                    Access::None);
            })
            .Execute([this, clearRadiance, clearCounters, clearNormals](rg::Resources &resources, CommandContext &cmd) {
                if (clearRadiance) {
                    auto radianceView = resources.GetImageView("Radiance");
                    vk::ClearColorValue clear;
                    vk::ImageSubresourceRange range;
                    range.layerCount = 1;
                    range.levelCount = 1;
                    range.aspectMask = vk::ImageAspectFlagBits::eColor;
                    cmd.Raw().clearColorImage(*radianceView->Image(),
                        vk::ImageLayout::eTransferDstOptimal,
                        clear,
                        {range});
                }
                if (clearNormals) {
                    auto normalsView = resources.GetImageView("Normals");
                    vk::ClearColorValue clear;
                    vk::ImageSubresourceRange range;
                    range.layerCount = 1;
                    range.levelCount = 1;
                    range.aspectMask = vk::ImageAspectFlagBits::eColor;
                    cmd.Raw().clearColorImage(*normalsView->Image(),
                        vk::ImageLayout::eTransferDstOptimal,
                        clear,
                        {range});
                }
                if (clearCounters) {
                    auto counterBuffer = resources.GetBuffer("FillCounters");
                    cmd.Raw().fillBuffer(*counterBuffer, 0, vk::WholeSize, 0);
                }

                auto listBuffer = resources.GetBuffer("FragmentListMetadata");
                for (size_t i = 0; i < fragmentListCount; i++) {
                    size_t offset = i * sizeof(GPUVoxelFragmentList);
                    cmd.Raw().fillBuffer(*listBuffer,
                        offset + offsetof(GPUVoxelFragmentList, count),
                        sizeof(uint32_t),
                        0);
                    cmd.Raw().fillBuffer(*listBuffer,
                        offset + offsetof(GPUVoxelFragmentList, capacity),
                        sizeof(uint32_t),
                        fragmentListSizes[i].capacity);
                    cmd.Raw().fillBuffer(*listBuffer,
                        offset + offsetof(GPUVoxelFragmentList, offset),
                        sizeof(uint32_t),
                        fragmentListSizes[i].offset);
                    cmd.Raw().fillBuffer(*listBuffer,
                        offset + offsetof(GPUVoxelFragmentList, cmd.x),
                        sizeof(uint32_t),
                        0);
                    cmd.Raw().fillBuffer(*listBuffer,
                        offset + offsetof(GPUVoxelFragmentList, cmd.y),
                        sizeof(uint32_t) * 2,
                        1);
                }
            });

        graph.AddPass("Fill")
            .Build([&](rg::PassBuilder &builder) {
                builder.Write("FillCounters", Access::FragmentShaderWrite);
                builder.Write("Radiance", Access::FragmentShaderWrite);
                builder.Write("Normals", Access::FragmentShaderWrite);

                builder.ReadPreviousFrame("VoxelState", Access::AnyShaderReadUniform);
                for (auto &voxelLayer : VoxelLayers[voxelFillIndex]) {
                    builder.ReadPreviousFrame(voxelLayer.fullName, Access::FragmentShaderSampleImage);
                }

                builder.ReadUniform("VoxelState");
                builder.ReadUniform("LightState");
                builder.Read("ShadowMap/Linear", Access::FragmentShaderSampleImage);

                builder.Write("FragmentListMetadata", Access::FragmentShaderWrite);
                builder.Write("FragmentLists", Access::FragmentShaderWrite);

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawID.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawID.drawParamsBuffer, Access::VertexShaderReadStorage);
            })

            .Execute([this, drawID, orthoAxes, voxelFillIndex](rg::Resources &resources, CommandContext &cmd) {
                ImageDesc desc;
                desc.extent = vk::Extent3D(std::max(voxelGridSize.x, voxelGridSize.z),
                    std::max(voxelGridSize.y, voxelGridSize.z),
                    1);
                desc.format = vk::Format::eR8Sint;
                desc.usage = vk::ImageUsageFlagBits::eColorAttachment;
                auto dummyTarget = resources.TemporaryImage(desc);

                cmd.ImageBarrier(dummyTarget->ImageView()->Image(),
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    {},
                    vk::PipelineStageFlagBits::eColorAttachmentOutput,
                    vk::AccessFlagBits::eColorAttachmentWrite);

                RenderPassInfo renderPass;
                renderPass.PushColorAttachment(dummyTarget->ImageView(), LoadOp::DontCare, StoreOp::DontCare);
                cmd.BeginRenderPass(renderPass);

                cmd.SetShaders("voxel_fill.vert", "voxel_fill.frag");

                GPUViewState lightViews[] = {{orthoAxes[0]}, {orthoAxes[1]}, {orthoAxes[2]}};
                cmd.UploadUniformData("ViewStates", lightViews, 3);

                std::array<vk::Rect2D, 3> viewports, scissors;
                for (size_t i = 0; i < viewports.size(); i++) {
                    viewports[i].extent = vk::Extent2D(orthoAxes[i].extents.x, orthoAxes[i].extents.y);
                    scissors[i].extent = vk::Extent2D(orthoAxes[i].extents.x, orthoAxes[i].extents.y);
                }
                cmd.SetViewportArray(viewports);
                cmd.SetScissorArray(scissors);
                cmd.SetCullMode(vk::CullModeFlagBits::eNone);
                cmd.SetDepthTest(false, false);

                cmd.SetUniformBuffer("VoxelStateUniform", resources.GetBuffer("VoxelState"));
                cmd.SetUniformBuffer("LightData", "LightState");
                cmd.SetImageView("shadowMap", "ShadowMap/Linear");
                cmd.SetStorageBuffer("FillCounters", "FillCounters");
                cmd.SetImageView("radianceOut", resources.GetImageMipView("Radiance", 0));
                cmd.SetImageView("normalsOut", resources.GetImageMipView("Normals", 0));
                cmd.SetStorageBuffer("VoxelFragmentListMetadata", "FragmentListMetadata");
                cmd.SetStorageBuffer("VoxelFragmentList", "FragmentLists");

                auto lastVoxelStateID = resources.GetID("VoxelState", false, 1);
                if (lastVoxelStateID != InvalidResource) {
                    cmd.SetUniformBuffer("PreviousVoxelStateUniform", lastVoxelStateID);
                } else {
                    cmd.SetUniformBuffer("PreviousVoxelStateUniform", "VoxelState");
                }
                for (auto &voxelLayer : VoxelLayers[voxelFillIndex]) {
                    auto lastVoxelLayerID = resources.GetID(voxelLayer.fullName, false, 1);
                    if (lastVoxelLayerID != InvalidResource) {
                        cmd.SetImageView(0, 10 + voxelLayer.dirIndex, resources.GetImageView(lastVoxelLayerID));
                    } else {
                        cmd.SetImageView(0, 10 + voxelLayer.dirIndex, resources.GetImageView("Radiance"));
                    }
                }

                cmd.SetShaderConstant(ShaderStage::Fragment, "FRAGMENT_LIST_COUNT", fragmentListCount);
                cmd.SetShaderConstant(ShaderStage::Fragment, "LIGHT_ATTENUATION", CVarLightAttenuation.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, "SHADOW_MAP_SAMPLE_WIDTH", CVarShadowMapSampleWidth.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, "SHADOW_MAP_SAMPLE_COUNT", CVarShadowMapSampleCount.Get());

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawID.drawCommandsBuffer),
                    resources.GetBuffer(drawID.drawParamsBuffer));

                cmd.EndRenderPass();
            });

        if (CVarEnableSpecularTracing.Get()) {
            for (uint32_t i = 1; i < fragmentListCount - 1; i++) {
                graph.AddPass("Merge")
                    .Build([&](rg::PassBuilder &builder) {
                        builder.Write("Radiance", Access::ComputeShaderWrite);
                        builder.Write("Normals", Access::ComputeShaderWrite);

                        builder.Read("FragmentListMetadata", Access::IndirectBuffer);
                        builder.Read("FragmentListMetadata", Access::ComputeShaderReadStorage);
                        builder.Read("FragmentLists", Access::ComputeShaderReadStorage);
                    })
                    .Execute([this, i](rg::Resources &resources, CommandContext &cmd) {
                        cmd.SetComputeShader("voxel_merge.comp");
                        cmd.SetShaderConstant(ShaderStage::Compute, "FRAGMENT_LIST_INDEX", i);

                        cmd.SetImageView("voxelRadiance", resources.GetImageMipView("Radiance", 0));
                        cmd.SetImageView("voxelNormals", resources.GetImageMipView("Normals", 0));

                        auto metadata = resources.GetBuffer("FragmentListMetadata");
                        cmd.SetStorageBuffer("VoxelFragmentListMetadata",
                            metadata,
                            i * sizeof(GPUVoxelFragmentList),
                            sizeof(GPUVoxelFragmentList));

                        cmd.SetStorageBuffer("VoxelFragmentList",
                            "FragmentLists",
                            fragmentListSizes[i].offset * sizeof(GPUVoxelFragment),
                            fragmentListSizes[i].capacity * sizeof(GPUVoxelFragment));

                        cmd.DispatchIndirect(metadata,
                            i * sizeof(GPUVoxelFragmentList) + offsetof(GPUVoxelFragmentList, cmd));
                    });
            }
            graph.AddPass("MergeSerial")
                .Build([&](rg::PassBuilder &builder) {
                    builder.Write("Radiance", Access::ComputeShaderWrite);
                    builder.Write("Normals", Access::ComputeShaderWrite);

                    builder.Read("FillCounters", Access::ComputeShaderReadStorage);
                    builder.Read("FragmentListMetadata", Access::IndirectBuffer);
                    builder.Read("FragmentListMetadata", Access::ComputeShaderReadStorage);
                    builder.Read("FragmentLists", Access::ComputeShaderReadStorage);
                })
                .Execute([this, i = fragmentListCount - 1](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetComputeShader("voxel_merge_serial.comp");
                    cmd.SetShaderConstant(ShaderStage::Compute, "FRAGMENT_LIST_COUNT", fragmentListCount);
                    cmd.SetUniformBuffer("VoxelStateUniform", resources.GetBuffer("VoxelState"));

                    cmd.SetImageView("voxelRadiance", resources.GetImageMipView("Radiance", 0));
                    cmd.SetImageView("voxelNormals", resources.GetImageMipView("Normals", 0));

                    auto metadata = resources.GetBuffer("FragmentListMetadata");
                    cmd.SetStorageBuffer("VoxelFragmentListMetadata",
                        metadata,
                        i * sizeof(GPUVoxelFragmentList),
                        sizeof(GPUVoxelFragmentList));

                    cmd.SetStorageBuffer("VoxelFragmentList",
                        "FragmentLists",
                        fragmentListSizes[i].offset * sizeof(GPUVoxelFragment),
                        fragmentListSizes[i].capacity * sizeof(GPUVoxelFragment));

                    cmd.SetStorageBuffer("FillCounters", "FillCounters");

                    cmd.Dispatch(1, 1, 1);
                });

            for (uint32_t i = 1; i < voxelGridMips; i++) {
                graph.AddPass("Mipmap")
                    .Build([&](rg::PassBuilder &builder) {
                        builder.Read("Radiance", Access::ComputeShaderSampleImage);
                        builder.Write("Radiance", Access::ComputeShaderWrite);
                        builder.Read("Normals", Access::ComputeShaderSampleImage);
                        builder.Write("Normals", Access::ComputeShaderWrite);
                    })
                    .Execute([this, i](rg::Resources &resources, CommandContext &cmd) {
                        cmd.SetComputeShader("voxel_mipmap.comp");

                        cmd.SetImageView("voxelRadianceIn", resources.GetImageMipView("Radiance", i - 1));
                        cmd.SetSampler("voxelRadianceIn", cmd.Device().GetSampler(SamplerType::TrilinearClampEdge));
                        cmd.SetImageView("voxelRadianceOut", resources.GetImageMipView("Radiance", i));

                        cmd.SetImageView("voxelNormalsIn", resources.GetImageMipView("Normals", i - 1));
                        cmd.SetSampler("voxelNormalsIn", cmd.Device().GetSampler(SamplerType::TrilinearClampEdge));
                        cmd.SetImageView("voxelNormalsOut", resources.GetImageMipView("Normals", i));

                        cmd.SetShaderConstant(ShaderStage::Compute, "MIP_LEVEL", i);

                        auto divisor = 8 << i;
                        auto dispatchCount = (voxelGridSize + divisor - 1) / divisor;
                        cmd.Dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
                    });
            }
        }

        AddBufferReadback(graph,
            "FragmentListMetadata",
            0,
            {},
            [this, listCount = fragmentListCount](BufferPtr buffer) {
                ZoneScopedN("FragmentListReadback");
                auto map = (const GPUVoxelFragmentList *)buffer->Mapped();
                bool printDebug = debugThisFrame[0].test();
                debugThisFrame[0].clear();
                for (uint32_t i = 0; i < listCount; i++) {
                    if (printDebug) {
                        Logf("fragment list %d, count: %u, capacity %u", i, map[i].count, map[i].capacity);
                    }
                    if (map[i].count > map[i].capacity) {
                        Warnf("fragment list %d overflow, count: %u, capacity: %u", i, map[i].count, map[i].capacity);
                    }
                }
            });
    }

    void Voxels::AddVoxelizationInit(RenderGraph &graph, const Lighting &lighting) {
        ZoneScoped;
        auto scope = graph.Scope("Voxels2");

        voxelLayerCount = std::min(CVarVoxelLayers.Get(), VoxelLayers.size());
        bool clearMipmap = (CVarVoxelClear.Get() & 8) == 8;

        if (voxelGridSize == glm::ivec3(0) || voxelLayerCount == 0 || !CVarEnableVoxels2.Get()) {
            for (size_t i = 0; i < VoxelLayers.size(); i++) {
                graph.AddPass("Dummy" + std::to_string(i))
                    .Build([i](rg::PassBuilder &builder) {
                        ImageDesc desc;
                        desc.extent = vk::Extent3D(1, 1, 1);
                        desc.primaryViewType = vk::ImageViewType::e3D;
                        desc.imageType = vk::ImageType::e3D;
                        desc.format = vk::Format::eR16G16B16A16Sfloat;
                        for (auto &layerInfo : VoxelLayers[i]) {
                            builder.CreateImage(layerInfo.name, desc, Access::TransferWrite);
                        }
                    })
                    .Execute([i](rg::Resources &resources, CommandContext &cmd) {
                        vk::ClearColorValue clear;
                        clear.setFloat32({0.0f, 0.0f, 0.0f, 1.0f});
                        vk::ImageSubresourceRange range;
                        range.layerCount = 1;
                        range.levelCount = 1;
                        range.aspectMask = vk::ImageAspectFlagBits::eColor;

                        for (auto &layerInfo : VoxelLayers[i]) {
                            auto layerView = resources.GetImageView(layerInfo.name);
                            if (!layerView) continue; // If the image is never read, it may be culled entirely.

                            cmd.Raw().clearColorImage(*layerView->Image(),
                                vk::ImageLayout::eTransferDstOptimal,
                                clear,
                                {range});
                        }

                        // updateDescriptorSet(resources, cmd.Device());
                    });
            }
            return;
        }

        graph.AddPass("Init")
            .Build([&](rg::PassBuilder &builder) {
                ImageDesc desc;
                desc.extent = vk::Extent3D(voxelGridSize.x, voxelGridSize.y, voxelGridSize.z);
                desc.primaryViewType = vk::ImageViewType::e3D;
                desc.imageType = vk::ImageType::e3D;
                desc.sampler = SamplerType::TrilinearClampBorder;
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                for (size_t layer = 0; layer < voxelLayerCount; layer++) {
                    for (auto &voxelLayer : VoxelLayers[layer]) {
                        builder.CreateImage(voxelLayer.name,
                            desc,
                            layer == 0 && clearMipmap ? Access::TransferWrite : Access::None);
                        builder.CreateImage(voxelLayer.preBlurName,
                            desc,
                            layer == 0 && clearMipmap ? Access::TransferWrite : Access::None);
                    }
                }
            })
            .Execute([clearMipmap](rg::Resources &resources, CommandContext &cmd) {
                if (clearMipmap) {
                    vk::ClearColorValue clear;
                    vk::ImageSubresourceRange range;
                    range.layerCount = 1;
                    range.levelCount = 1;
                    range.aspectMask = vk::ImageAspectFlagBits::eColor;

                    for (auto &voxelLayer : VoxelLayers[0]) {
                        auto layerView = resources.GetImageView(voxelLayer.name);
                        if (layerView) {
                            cmd.Raw().clearColorImage(*layerView->Image(),
                                vk::ImageLayout::eTransferDstOptimal,
                                clear,
                                {range});
                        }

                        auto layerView2 = resources.GetImageView(voxelLayer.preBlurName);
                        if (layerView2) {
                            cmd.Raw().clearColorImage(*layerView2->Image(),
                                vk::ImageLayout::eTransferDstOptimal,
                                clear,
                                {range});
                        }
                    }
                }

                // updateDescriptorSet(resources, cmd.Device());
            });
    }

    void Voxels::AddVoxelization2(RenderGraph &graph, const Lighting &lighting) {
        if (voxelGridSize == glm::ivec3(0) || voxelLayerCount == 0 || !CVarEnableVoxels.Get() ||
            !CVarEnableVoxels2.Get()) {
            return;
        }

        ZoneScoped;
        auto scope = graph.Scope("Voxels2");

        struct GPULayerData {
            glm::vec3 direction;
            uint32_t layerIndex;
        };
        static_assert(sizeof(GPULayerData) == sizeof(glm::vec4), "GPULayerData size missmatch");

        graph.AddPass("VoxelFill")
            .Build([&](rg::PassBuilder &builder) {
                for (auto &voxelLayer : VoxelLayers[0]) {
                    builder.Write(voxelLayer.name, Access::ComputeShaderWrite);
                }

                builder.ReadUniform("VoxelState");

                builder.Read("Voxels/FragmentListMetadata", Access::IndirectBuffer);
                builder.Read("Voxels/FragmentListMetadata", Access::ComputeShaderReadStorage);
                builder.Read("Voxels/FragmentLists", Access::ComputeShaderReadStorage);
            })

            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetComputeShader("voxel_fill_layer.comp");

                cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");

                auto metadata = resources.GetBuffer("Voxels/FragmentListMetadata");
                cmd.SetStorageBuffer("VoxelFragmentListMetadata", metadata, 0, sizeof(GPUVoxelFragmentList));

                cmd.SetStorageBuffer("VoxelFragmentList",
                    "Voxels/FragmentLists",
                    fragmentListSizes[0].offset * sizeof(GPUVoxelFragment),
                    fragmentListSizes[0].capacity * sizeof(GPUVoxelFragment));

                for (size_t i = 0; i < directions.size(); i++) {
                    cmd.SetImageView(0, 3 + i, resources.GetImageView(VoxelLayers[0][i].name));
                }

                cmd.DispatchIndirect(metadata, offsetof(GPUVoxelFragmentList, cmd));
            });

        for (uint32_t i = 1; i < fragmentListCount - 1; i++) {
            graph.AddPass("Merge")
                .Build([&](rg::PassBuilder &builder) {
                    for (auto &voxelLayer : VoxelLayers[0]) {
                        builder.Read(voxelLayer.name, Access::ComputeShaderReadStorage);
                        builder.Write(voxelLayer.name, Access::ComputeShaderWrite);
                    }

                    builder.ReadUniform("VoxelState");

                    builder.Read("Voxels/FragmentListMetadata", Access::IndirectBuffer);
                    builder.Read("Voxels/FragmentListMetadata", Access::ComputeShaderReadStorage);
                    builder.Read("Voxels/FragmentLists", Access::ComputeShaderReadStorage);
                })
                .Execute([this, i](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetComputeShader("voxel_merge_layer.comp");
                    cmd.SetShaderConstant(ShaderStage::Compute, "FRAGMENT_LIST_INDEX", i);

                    cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");

                    auto metadata = resources.GetBuffer("Voxels/FragmentListMetadata");
                    cmd.SetStorageBuffer("VoxelFragmentListMetadata",
                        metadata,
                        i * sizeof(GPUVoxelFragmentList),
                        sizeof(GPUVoxelFragmentList));

                    cmd.SetStorageBuffer("VoxelFragmentList",
                        "Voxels/FragmentLists",
                        fragmentListSizes[i].offset * sizeof(GPUVoxelFragment),
                        fragmentListSizes[i].capacity * sizeof(GPUVoxelFragment));

                    for (size_t j = 0; j < directions.size(); j++) {
                        cmd.SetImageView(0, 3 + j, resources.GetImageView(VoxelLayers[0][j].name));
                    }

                    cmd.DispatchIndirect(metadata,
                        i * sizeof(GPUVoxelFragmentList) + offsetof(GPUVoxelFragmentList, cmd));
                });
        }
        graph.AddPass("MergeSerial")
            .Build([&](rg::PassBuilder &builder) {
                for (auto &voxelLayer : VoxelLayers[0]) {
                    builder.Read(voxelLayer.name, Access::ComputeShaderReadStorage);
                    builder.Write(voxelLayer.name, Access::ComputeShaderWrite);
                }

                builder.ReadUniform("VoxelState");

                builder.Read("Voxels/FragmentListMetadata", Access::IndirectBuffer);
                builder.Read("Voxels/FragmentListMetadata", Access::ComputeShaderReadStorage);
                builder.Read("Voxels/FragmentLists", Access::ComputeShaderReadStorage);
            })
            .Execute([this, i = fragmentListCount - 1](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetComputeShader("voxel_merge_layer_serial.comp");
                cmd.SetShaderConstant(ShaderStage::Compute, "FRAGMENT_LIST_COUNT", fragmentListCount);

                cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");

                auto metadata = resources.GetBuffer("Voxels/FragmentListMetadata");
                cmd.SetStorageBuffer("VoxelFragmentListMetadata",
                    metadata,
                    i * sizeof(GPUVoxelFragmentList),
                    sizeof(GPUVoxelFragmentList));

                cmd.SetStorageBuffer("VoxelFragmentList",
                    "Voxels/FragmentLists",
                    fragmentListSizes[i].offset * sizeof(GPUVoxelFragment),
                    fragmentListSizes[i].capacity * sizeof(GPUVoxelFragment));

                for (size_t j = 0; j < directions.size(); j++) {
                    cmd.SetImageView(0, 3 + j, resources.GetImageView(VoxelLayers[0][j].name));
                }

                cmd.Dispatch(1, 1, 1);
            });

        for (size_t layer = 1; layer < voxelLayerCount; layer++) {
            graph.AddPass("PreBlur")
                .Build([&](rg::PassBuilder &builder) {
                    builder.ReadUniform("VoxelState");
                    builder.ReadPreviousFrame("VoxelState", Access::AnyShaderReadUniform);

                    for (auto &voxelLayer : VoxelLayers[layer]) {
                        builder.Write(voxelLayer.preBlurName, Access::ComputeShaderWrite);

                        auto &prevLayer = VoxelLayers[layer - 1][voxelLayer.dirIndex].name;
                        builder.Read(prevLayer, Access::ComputeShaderSampleImage);

                        auto &lastFrameOutput = VoxelLayers[voxelLayerCount - 1][voxelLayer.dirIndex].preBlurName;
                        builder.ReadPreviousFrame(lastFrameOutput, Access::FragmentShaderSampleImage);
                    }
                })
                .Execute([this, layer](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetComputeShader("voxel_mipmap_layer.comp");
                    cmd.SetShaderConstant(ShaderStage::Compute,
                        "PREVIOUS_FRAME_BLEND",
                        layer == 1 ? CVarLightLowPass.Get() : 0.0f);
                    cmd.SetShaderConstant(ShaderStage::Compute, "LAYER_INDEX", (uint32_t)layer);

                    cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");

                    auto lastVoxelStateID = resources.GetID("VoxelState", false, 1);
                    if (lastVoxelStateID != InvalidResource && CVarReprojectVoxelGrid.Get()) {
                        cmd.SetUniformBuffer("PreviousVoxelStateUniform", lastVoxelStateID);
                    } else {
                        cmd.SetUniformBuffer("PreviousVoxelStateUniform", "VoxelState");
                    }

                    for (size_t i = 0; i < directions.size(); i++) {
                        auto &prevLayer = VoxelLayers[layer - 1][i].name;
                        cmd.SetImageView(0, 2 + i, resources.GetImageView(prevLayer));

                        cmd.SetImageView(0,
                            2 + directions.size() + i,
                            resources.GetImageView(VoxelLayers[layer][i].preBlurName));

                        auto &lastFrameOutput = VoxelLayers[voxelLayerCount - 1][i].preBlurName;
                        auto lastVoxelLayerID = resources.GetID(lastFrameOutput, false, 1);
                        if (lastVoxelLayerID != InvalidResource) {
                            cmd.SetImageView(0,
                                2 + directions.size() * 2 + i,
                                resources.GetImageView(lastVoxelLayerID));
                        } else {
                            cmd.SetImageView(0, 2 + directions.size() * 2 + i, resources.GetImageView(prevLayer));
                        }
                    }

                    auto dispatchCount = (voxelGridSize + 7) / 8;
                    cmd.Dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
                });

            graph.AddPass("BlurLayer")
                .Build([&](rg::PassBuilder &builder) {
                    for (auto &voxelLayer : VoxelLayers[layer]) {
                        builder.Read(voxelLayer.preBlurName, Access::ComputeShaderSampleImage);
                        builder.Write(voxelLayer.name, Access::ComputeShaderWrite);
                    }

                    for (auto &voxelLayer : VoxelLayers[layer - 1]) {
                        builder.Read(voxelLayer.preBlurName, Access::ComputeShaderSampleImage);
                    }
                })
                .Execute([this, layer](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetComputeShader("voxel_mipmap_layer_blur.comp");

                    cmd.SetShaderConstant(ShaderStage::Compute, "LAYER_INDEX", (uint32_t)layer);

                    for (size_t i = 0; i < directions.size(); i++) {
                        cmd.SetImageView(0, i, resources.GetImageView(VoxelLayers[layer][i].name));
                        cmd.SetImageView(0,
                            directions.size() + i,
                            resources.GetImageView(VoxelLayers[layer][i].preBlurName));

                        cmd.SetImageView(0,
                            directions.size() * 2 + i,
                            resources.GetImageView(VoxelLayers[layer - 1][i].preBlurName));
                    }

                    auto dispatchCount = (voxelGridSize + 7) / 8;
                    cmd.Dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
                });
        }
    }

    void Voxels::AddMarchingCubes(RenderGraph &graph) {
        uint32_t version = CVarEnableMarchingCubes.Get();
        if (voxelGridSize == glm::ivec3(0) || voxelLayerCount == 0 || !CVarEnableVoxels.Get() ||
            !CVarEnableVoxels2.Get() || version == 0) {
            return;
        }
        uint32_t cubeLayer = CVarMarchingCubesLayer.Get();
        cubeLayer = std::min(cubeLayer, voxelLayerCount - 1);
        glm::uvec3 cubeGridSize = glm::max(glm::ivec3(1), voxelGridSize - 1);
        cubeGridSize >>= cubeLayer;
        size_t cubeCount = cubeGridSize.x * cubeGridSize.y * cubeGridSize.z;

        ZoneScoped;
        auto scope = graph.Scope("MarchingCubes");

        graph.AddPass("Init")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateBuffer("VertexBuffer",
                    {sizeof(PositionVertex), 4 * cubeCount},
                    Residency::GPU_ONLY,
                    Access::None);

                builder.CreateBuffer("IndexLookup",
                    {sizeof(uint32_t), 2 + 4 * cubeCount},
                    Residency::GPU_ONLY,
                    Access::TransferWrite);

                builder.CreateBuffer("IndexBuffer",
                    {sizeof(uint32_t), (sizeof(VkDrawIndexedIndirectCommand) / sizeof(uint32_t)) + 5 * cubeCount},
                    Residency::GPU_ONLY,
                    Access::TransferWrite);
            })
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
                static uint32_t time = 0;
                auto lookupBuffer = resources.GetBuffer("IndexLookup");
                cmd.Raw().fillBuffer(*lookupBuffer, 0, sizeof(uint32_t), time++);
                cmd.Raw().fillBuffer(*lookupBuffer, sizeof(uint32_t), sizeof(uint32_t), 1u);
                cmd.Raw().fillBuffer(*lookupBuffer, 2 * sizeof(uint32_t), sizeof(glm::vec3), 0u);
                auto indexBuffer = resources.GetBuffer("IndexBuffer");
                cmd.Raw().fillBuffer(*indexBuffer, 0, sizeof(VkDrawIndexedIndirectCommand), 0u);
                cmd.Raw().fillBuffer(*indexBuffer,
                    offsetof(VkDrawIndexedIndirectCommand, instanceCount),
                    sizeof(uint32_t),
                    1u);
            });
        graph.AddPass("UploadTriangleData")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateBuffer("TriangleIndexBuffer",
                    {sizeof(triangleIndexBuffer[0]), sizeof(triangleIndexBuffer) / sizeof(triangleIndexBuffer[0])},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
                builder.CreateBuffer("CaseInteriorEdges",
                    {sizeof(caseInteriorEdge[0][0]), sizeof(caseInteriorEdge) / sizeof(caseInteriorEdge[0][0])},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
                builder.CreateBuffer("TriangleCaseOffsets",
                    {sizeof(triangleCaseOffset[0][0][0]),
                        sizeof(triangleCaseOffset) / sizeof(triangleCaseOffset[0][0][0])},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
                builder.CreateBuffer("CaseTests",
                    {sizeof(caseTests[0][0]), sizeof(caseTests) / sizeof(caseTests[0][0])},
                    Residency::CPU_TO_GPU,
                    Access::HostWrite);
            })
            .Execute([](rg::Resources &resources, DeviceContext &device) {
                resources.GetBuffer("TriangleIndexBuffer")
                    ->CopyFrom(&triangleIndexBuffer[0], sizeof(triangleIndexBuffer) / sizeof(triangleIndexBuffer[0]));
                resources.GetBuffer("CaseInteriorEdges")
                    ->CopyFrom(&caseInteriorEdge[0][0], sizeof(caseInteriorEdge) / sizeof(caseInteriorEdge[0][0]));
                resources.GetBuffer("TriangleCaseOffsets")
                    ->CopyFrom(&triangleCaseOffset[0][0][0],
                        sizeof(triangleCaseOffset) / sizeof(triangleCaseOffset[0][0][0]));
                resources.GetBuffer("CaseTests")
                    ->CopyFrom(&caseTests[0][0], sizeof(caseTests) / sizeof(caseTests[0][0]));
            });

        graph.AddPass("MarchChunkVertex")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("Voxels/Radiance", Access::ComputeShaderSampleImage);

                builder.Read("IndexLookup", Access::ComputeShaderReadStorage);
                builder.Write("IndexLookup", Access::ComputeShaderWrite);
                builder.Write("VertexBuffer", Access::ComputeShaderWrite);
                builder.ReadUniform("VoxelState");
            })

            .Execute([cubeLayer, cubeGridSize, version](rg::Resources &resources, CommandContext &cmd) {
                if (version == 2) {
                    cmd.SetComputeShader("marching_cubes_vertex2.comp");
                } else {
                    cmd.SetComputeShader("marching_cubes_vertex.comp");
                }
                cmd.SetShaderConstant(ShaderStage::Compute, "CUBE_LAYER", cubeLayer);

                cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");

                cmd.SetImageView("voxelRadiance", resources.GetImageMipView("Voxels/Radiance", cubeLayer));
                cmd.SetStorageBuffer("IndexLookupBuffer", "IndexLookup");
                cmd.SetStorageBuffer("VertexBuffer", "VertexBuffer");

                glm::uvec3 groups = (cubeGridSize + 7u) / 8u;
                cmd.Dispatch(groups.x, groups.y, groups.z);
            });

        graph.AddPass("MarchChunkTriangle")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("Voxels/Radiance", Access::ComputeShaderSampleImage);

                builder.Read("IndexLookup", Access::ComputeShaderReadStorage);
                builder.Read("IndexBuffer", Access::ComputeShaderReadStorage);
                builder.Write("IndexBuffer", Access::ComputeShaderWrite);
                builder.Read("TriangleIndexBuffer", Access::ComputeShaderReadStorage);
                builder.Read("CaseInteriorEdges", Access::ComputeShaderReadStorage);
                builder.Read("TriangleCaseOffsets", Access::ComputeShaderReadStorage);
                builder.Read("CaseTests", Access::ComputeShaderReadStorage);
                builder.ReadUniform("VoxelState");
            })

            .Execute([cubeLayer, cubeGridSize, version](rg::Resources &resources, CommandContext &cmd) {
                if (version == 2) {
                    cmd.SetComputeShader("marching_cubes_triangle2.comp");

                    cmd.SetStorageBuffer("TriangleIndexBuffer", "TriangleIndexBuffer");
                    cmd.SetStorageBuffer("CaseInteriorEdges", "CaseInteriorEdges");
                    cmd.SetStorageBuffer("TriangleCaseOffsets", "TriangleCaseOffsets");
                    cmd.SetStorageBuffer("CaseTests", "CaseTests");
                } else {
                    cmd.SetComputeShader("marching_cubes_triangle.comp");
                }
                cmd.SetShaderConstant(ShaderStage::Compute, "CUBE_LAYER", cubeLayer);

                cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");

                cmd.SetImageView("voxelRadiance", resources.GetImageMipView("Voxels/Radiance", cubeLayer));
                cmd.SetStorageBuffer("IndexLookupBuffer", "IndexLookup");
                cmd.SetStorageBuffer("IndexBuffer", "IndexBuffer");

                glm::uvec3 groups = (cubeGridSize + 7u) / 8u;
                cmd.Dispatch(groups.x, groups.y, groups.z);
            });

        bool printDebug = debugThisFrame[1].test();
        debugThisFrame[1].clear();
        if (printDebug) {
            AddBufferReadback(graph, "IndexBuffer", 0, sizeof(uint32_t) * (1 + 3 * cubeCount), [](BufferPtr buffer) {
                ZoneScopedN("IndexBufferReadback");
                const uint32_t *indexData = (const uint32_t *)buffer->Mapped();
                uint32_t triangleCount = indexData[0];
                Logf("Triangle count: %u", triangleCount);
                // for (uint32_t triangleIndex = 0; triangleIndex < triangleCount; triangleIndex++) {
                //     Logf("Triangle %u = [%u %u %u]",
                //         triangleIndex,
                //         indexData[triangleIndex * 3],
                //         indexData[triangleIndex * 3 + 1],
                //         indexData[triangleIndex * 3 + 2]);
                // }
            });
        }
    }

    void Voxels::AddDebugPass(RenderGraph &graph) {
        if (graph.HasResource("MarchingCubes/IndexBuffer")) {
            graph.AddPass("DrawChunk")
                .Build([&](rg::PassBuilder &builder) {
                    builder.ReadUniform("ViewState");
                    builder.ReadUniform("VoxelState");
                    builder.Read("ExposureState", Access::FragmentShaderReadStorage);
                    builder.Read("MarchingCubes/VertexBuffer", Access::VertexBuffer);
                    builder.Read("MarchingCubes/IndexBuffer", Access::IndexBuffer);
                    builder.Read("MarchingCubes/IndexBuffer", Access::IndirectBuffer);

                    builder.SetColorAttachment(0, builder.LastOutputID(), {LoadOp::Load, StoreOp::Store});
                    builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});
                })
                .Execute([](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetShaders("cubescene.vert", "depth_colored.frag");
                    cmd.SetUniformBuffer("ViewStates", "ViewState");
                    cmd.SetStorageBuffer("ExposureState", "ExposureState");

                    auto vertexBuffer = resources.GetBuffer("MarchingCubes/VertexBuffer");
                    auto indexBuffer = resources.GetBuffer("MarchingCubes/IndexBuffer");

                    cmd.SetVertexLayout(PositionVertex::Layout());
                    cmd.Raw().bindIndexBuffer(*indexBuffer,
                        sizeof(VkDrawIndexedIndirectCommand),
                        vk::IndexType::eUint32);
                    cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});

                    cmd.DrawIndexedIndirect(indexBuffer, 0u, 1u);
                });
        }

        if (CVarVoxelDebug.Get() <= 0 || voxelGridSize == glm::ivec3(0)) return;
        auto debugMipLayer = std::min(CVarVoxelDebugMip.Get(), voxelLayerCount - 1);

        graph.AddPass("VoxelDebug")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("Voxels/FillCounters", Access::FragmentShaderReadStorage);
                builder.Read("Voxels/Radiance", Access::FragmentShaderSampleImage);
                builder.Read("Voxels/Normals", Access::FragmentShaderSampleImage);
                builder.ReadUniform("ViewState");
                builder.ReadUniform("VoxelState");
                builder.Read("ExposureState", Access::FragmentShaderReadStorage);

                for (auto &voxelLayer : VoxelLayers[debugMipLayer]) {
                    builder.Read(voxelLayer.fullName, Access::FragmentShaderSampleImage);
                }

                builder.Read(builder.LastOutputID(), Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveImage(builder.LastOutputID());
                builder.OutputColorAttachment(0, "VoxelDebug", desc, {LoadOp::DontCare, StoreOp::Store});
                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::ReadOnly});
            })
            .Execute([debugMipLayer](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "voxel_debug.frag");
                cmd.SetStencilTest(true);
                cmd.SetDepthTest(false, false);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetUniformBuffer("ViewStates", "ViewState");
                cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");
                cmd.SetStorageBuffer("ExposureState", "ExposureState");
                cmd.SetImageView("overlayTex", resources.LastOutputID());
                cmd.SetStorageBuffer("FillCounters", "Voxels/FillCounters");
                cmd.SetImageView("voxelRadiance", "Voxels/Radiance");
                cmd.SetImageView("voxelNormals", "Voxels/Normals");

                for (auto &voxelLayer : VoxelLayers[debugMipLayer]) {
                    auto layerView = resources.GetImageView(voxelLayer.fullName);
                    Assertf(layerView, "Layer view missing: %s", voxelLayer.fullName);
                    cmd.SetImageView(1, voxelLayer.dirIndex, layerView);
                }

                cmd.SetShaderConstant(ShaderStage::Fragment, "DEBUG_MODE", CVarVoxelDebug.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, "BLEND_WEIGHT", CVarVoxelDebugBlend.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, "VOXEL_MIP", debugMipLayer);

                cmd.Draw(3);
            });
    }

    vk::DescriptorSet Voxels::GetCurrentVoxelDescriptorSet() const {
        return layerDescriptorSets[currentSetFrame];
    }
} // namespace sp::vulkan::renderer
