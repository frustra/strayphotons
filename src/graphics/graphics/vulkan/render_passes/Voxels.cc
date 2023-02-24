#include "Voxels.hh"

#include "core/Common.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"
#include "graphics/vulkan/render_passes/Lighting.hh"
#include "graphics/vulkan/render_passes/Readback.hh"

namespace sp::vulkan::renderer {
    static CVar<bool> CVarEnableVoxels("r.EnableVoxels", true, "Enable world voxelization for lighting");
    static CVar<bool> CVarEnableVoxels2("r.EnableVoxels2", true, "Enable world voxelization for lighting");
    static CVar<int> CVarVoxelDebug("r.VoxelDebug",
        0,
        "Enable voxel grid debug view (0: off, 1: ray march, 2: cone trace, 3: diffuse trace)");
    static CVar<float> CVarVoxelDebugBlend("r.VoxelDebugBlend", 0.0f, "The blend weight used to overlay voxel debug");
    static CVar<uint32_t> CVarVoxelDebugMip("r.VoxelDebugMip", 0, "The voxel mipmap to sample in the debug view");
    static CVar<uint32_t> CVarVoxelLayers("r.VoxelLayers", 8, "The number of voxel mipmap layers");
    static CVar<int> CVarVoxelClear("r.VoxelClear",
        15,
        "Change the voxel grid clearing operation used between frames "
        "(bitfield: 1=radiance, 2=counters, 4=normals, 8=mipmap)");
    static CVar<float> CVarLightAttenuation("r.LightAttenuation", 0.5, "Light attenuation for voxel bounces");

    static CVar<uint32> CVarVoxelFragmentBuckets("r.VoxelFragmentBuckets",
        9,
        "The number of fragments that can be written to a voxel.");

    static CVar<uint32> CVarVoxelFragmentBucketSizeFactor("r.VoxelFragmentBucketSizeFactor",
        2,
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

    void Voxels::LoadState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::VoxelArea, ecs::TransformSnapshot>> lock) {
        voxelGridSize = glm::ivec3(0);
        for (auto entity : lock.EntitiesWith<ecs::VoxelArea>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &area = entity.Get<ecs::VoxelArea>(lock);
            if (area.extents.x > 0 && area.extents.y > 0 && area.extents.z > 0) {
                voxelGridSize = area.extents;
                voxelToWorld = entity.Get<ecs::TransformSnapshot>(lock);
                break; // Only 1 voxel area supported for now
            }
        }

        graph.AddPass("VoxelState")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateUniform("VoxelState", sizeof(GPUVoxelState));
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                GPUVoxelState gpuData = {glm::inverse(voxelToWorld.GetMatrix()), voxelGridSize};
                resources.GetBuffer("VoxelState")->CopyFrom(&gpuData);
            });
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
        axisTransform[0].Rotate(M_PI_2, glm::vec3(0, 1, 0));
        axisTransform[1].Rotate(M_PI_2, glm::vec3(1, 0, 0));

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

        bool clearRadiance = (CVarVoxelClear.Get() & 1) == 1;
        bool clearCounters = (CVarVoxelClear.Get() & 2) == 2;
        bool clearNormals = (CVarVoxelClear.Get() & 4) == 4;
        auto drawID = scene.GenerateDrawsForView(graph, ortho.visibilityMask, 3);

        auto voxelGridExtents = vk::Extent3D(voxelGridSize.x, voxelGridSize.y, voxelGridSize.z);
        auto voxelGridMips = CalculateMipmapLevels(voxelGridExtents);

        fragmentListCount = std::min(MAX_VOXEL_FRAGMENT_LISTS, CVarVoxelFragmentBuckets.Get());

        uint32 totalFragmentListSize = 0;
        {
            auto fragmentListSize = (voxelGridSize.x * voxelGridSize.y * voxelGridSize.z) / 2;
            for (uint32 i = 0; i < fragmentListCount; i++) {
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

                desc.format = vk::Format::eR32Uint;
                builder.CreateImage("FillCounters", desc, clearCounters ? Access::TransferWrite : Access::None);

                desc.sampler = SamplerType::TrilinearClampBorder;
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.CreateImage("Radiance", desc, clearRadiance ? Access::TransferWrite : Access::None);
                // desc.format = vk::Format::eR8G8B8A8Snorm;
                builder.CreateImage("Normals", desc, clearNormals ? Access::TransferWrite : Access::None);

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
                if (clearCounters) {
                    auto counterView = resources.GetImageView("FillCounters");
                    vk::ClearColorValue clear;
                    vk::ImageSubresourceRange range;
                    range.layerCount = 1;
                    range.levelCount = counterView->MipLevels();
                    range.aspectMask = vk::ImageAspectFlagBits::eColor;
                    cmd.Raw().clearColorImage(*counterView->Image(),
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
                builder.ReadPreviousFrame("Radiance", Access::FragmentShaderSampleImage);
                builder.ReadPreviousFrame("Normals", Access::FragmentShaderSampleImage);

                builder.ReadUniform("VoxelState");
                builder.ReadUniform("LightState");
                builder.Read("ShadowMap.Linear", Access::FragmentShaderSampleImage);

                builder.Write("FragmentListMetadata", Access::FragmentShaderWrite);
                builder.Write("FragmentLists", Access::FragmentShaderWrite);

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawID.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawID.drawParamsBuffer, Access::VertexShaderReadStorage);
            })

            .Execute([this, drawID, orthoAxes](rg::Resources &resources, CommandContext &cmd) {
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
                cmd.UploadUniformData(0, 0, lightViews, 3);

                std::array<vk::Rect2D, 3> viewports, scissors;
                for (size_t i = 0; i < viewports.size(); i++) {
                    viewports[i].extent = vk::Extent2D(orthoAxes[i].extents.x, orthoAxes[i].extents.y);
                    scissors[i].extent = vk::Extent2D(orthoAxes[i].extents.x, orthoAxes[i].extents.y);
                }
                cmd.SetViewportArray(viewports);
                cmd.SetScissorArray(scissors);
                cmd.SetCullMode(vk::CullModeFlagBits::eNone);

                cmd.SetUniformBuffer(0, 1, resources.GetBuffer("VoxelState"));
                cmd.SetUniformBuffer(0, 2, resources.GetBuffer("LightState"));
                cmd.SetImageView(0, 3, resources.GetImageView("ShadowMap.Linear"));
                cmd.SetImageView(0, 4, resources.GetImageMipView("FillCounters", 0));
                cmd.SetImageView(0, 5, resources.GetImageMipView("Radiance", 0));
                cmd.SetImageView(0, 6, resources.GetImageMipView("Normals", 0));
                cmd.SetStorageBuffer(0, 7, resources.GetBuffer("FragmentListMetadata"));
                cmd.SetStorageBuffer(0, 8, resources.GetBuffer("FragmentLists"));

                auto lastRadianceID = resources.GetID("Radiance", false, 1);
                if (lastRadianceID != InvalidResource) {
                    cmd.SetImageView(0, 9, resources.GetImageMipView(lastRadianceID, 0));
                } else {
                    cmd.SetImageView(0, 9, resources.GetImageMipView("Radiance", 0));
                }
                auto lastNormalsID = resources.GetID("Normals", false, 1);
                if (lastNormalsID != InvalidResource) {
                    cmd.SetImageView(0, 10, resources.GetImageMipView(lastNormalsID, 0));
                } else {
                    cmd.SetImageView(0, 10, resources.GetImageMipView("Normals", 0));
                }

                cmd.SetShaderConstant(ShaderStage::Fragment, 0, fragmentListCount);
                cmd.SetShaderConstant(ShaderStage::Fragment, 1, CVarLightAttenuation.Get());

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawID.drawCommandsBuffer),
                    resources.GetBuffer(drawID.drawParamsBuffer));

                cmd.EndRenderPass();
            });

        for (uint32_t i = 1; i < fragmentListCount; i++) {
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
                    cmd.SetShaderConstant(ShaderStage::Compute, 0, i);

                    cmd.SetImageView(0, 0, resources.GetImageMipView("Radiance", 0));
                    cmd.SetImageView(0, 1, resources.GetImageMipView("Normals", 0));

                    auto metadata = resources.GetBuffer("FragmentListMetadata");
                    cmd.SetStorageBuffer(0,
                        2,
                        metadata,
                        i * sizeof(GPUVoxelFragmentList),
                        sizeof(GPUVoxelFragmentList));

                    cmd.SetStorageBuffer(0,
                        3,
                        resources.GetBuffer("FragmentLists"),
                        fragmentListSizes[i].offset * sizeof(GPUVoxelFragment),
                        fragmentListSizes[i].capacity * sizeof(GPUVoxelFragment));

                    cmd.DispatchIndirect(metadata,
                        i * sizeof(GPUVoxelFragmentList) + offsetof(GPUVoxelFragmentList, cmd));
                });
        }

        for (uint32_t i = 1; i < voxelGridMips; i++) {
            graph.AddPass("Mipmap")
                .Build([&](rg::PassBuilder &builder) {
                    builder.Write("Radiance", Access::ComputeShaderWrite);
                })
                .Execute([this, i](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetComputeShader("voxel_mipmap.comp");

                    cmd.SetImageView(0, 0, resources.GetImageMipView("Radiance", i - 1));
                    cmd.SetSampler(0, 0, cmd.Device().GetSampler(SamplerType::TrilinearClampEdge));
                    cmd.SetImageView(0, 1, resources.GetImageMipView("Radiance", i));

                    cmd.SetImageView(0, 2, resources.GetImageMipView("Normals", i - 1));
                    cmd.SetSampler(0, 2, cmd.Device().GetSampler(SamplerType::TrilinearClampEdge));
                    cmd.SetImageView(0, 3, resources.GetImageMipView("Normals", i));

                    cmd.SetShaderConstant(ShaderStage::Compute, 0, i);

                    auto divisor = 8 << i;
                    auto dispatchCount = (voxelGridSize + divisor - 1) / divisor;
                    cmd.Dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
                });
        }

        AddBufferReadback(graph, "FragmentListMetadata", 0, {}, [listCount = fragmentListCount](BufferPtr buffer) {
            auto map = (const GPUVoxelFragmentList *)buffer->Mapped();
            for (uint32 i = 0; i < listCount; i++) {
                Assertf(map[i].count <= map[i].capacity,
                    "fragment list %d overflow, count: %u, capacity: %u",
                    i,
                    map[i].count,
                    map[i].capacity);
            }
        });
    }

    void Voxels::AddVoxelization2(RenderGraph &graph, const Lighting &lighting) {
        ZoneScoped;
        auto scope = graph.Scope("Voxels2");

        if (voxelGridSize == glm::ivec3(0) || !CVarEnableVoxels2.Get()) {
            graph.AddPass("Dummy")
                .Build([&](rg::PassBuilder &builder) {
                    ImageDesc desc;
                    desc.extent = vk::Extent3D(1, 1, 1);
                    desc.primaryViewType = vk::ImageViewType::e3D;
                    desc.imageType = vk::ImageType::e3D;
                    desc.format = vk::Format::eR16G16B16A16Sfloat;
                    for (auto &layerInfo : VoxelLayers) {
                        builder.CreateImage(layerInfo.name, desc, Access::TransferWrite);
                    }
                })
                .Execute([](rg::Resources &resources, CommandContext &cmd) {
                    vk::ClearColorValue clear;
                    clear.setFloat32({0.0f, 0.0f, 0.0f, 1.0f});
                    vk::ImageSubresourceRange range;
                    range.layerCount = 1;
                    range.levelCount = 1;
                    range.aspectMask = vk::ImageAspectFlagBits::eColor;

                    for (auto &layerInfo : VoxelLayers) {
                        auto layerView = resources.GetImageView(layerInfo.name);
                        if (!layerView) continue; // If the image is never read, it may be culled entirely.

                        cmd.Raw().clearColorImage(*layerView->Image(),
                            vk::ImageLayout::eTransferDstOptimal,
                            clear,
                            {range});
                    }
                });
            return;
        }

        // ecs::View ortho;
        // ortho.visibilityMask = ecs::VisibilityMask::LightingVoxel;

        // auto voxelCenter = voxelToWorld;
        // voxelCenter.Translate(voxelCenter * (0.5f * glm::vec4(voxelGridSize, 0)));

        // std::array<ecs::Transform, 3> axisTransform = {voxelCenter, voxelCenter, voxelCenter};
        // axisTransform[0].Rotate(M_PI_2, glm::vec3(0, 1, 0));
        // axisTransform[1].Rotate(M_PI_2, glm::vec3(1, 0, 0));

        // axisTransform[0].Scale(glm::vec3(voxelGridSize.z, voxelGridSize.y, voxelGridSize.x));
        // axisTransform[1].Scale(glm::vec3(voxelGridSize.x, voxelGridSize.z, voxelGridSize.y));
        // axisTransform[2].Scale(voxelGridSize);

        // std::array<ecs::View, 3> orthoAxes = {ortho, ortho, ortho};
        // orthoAxes[0].extents = glm::ivec2(voxelGridSize.z, voxelGridSize.y);
        // orthoAxes[1].extents = glm::ivec2(voxelGridSize.x, voxelGridSize.z);
        // orthoAxes[2].extents = glm::ivec2(voxelGridSize.x, voxelGridSize.y);
        // for (size_t i = 0; i < orthoAxes.size(); i++) {
        //     auto axis = axisTransform[i];
        //     axis.Scale(glm::vec3(0.5, 0.5, 1));
        //     axis.Translate(axis * glm::vec4(0, 0, -0.5, 0));
        //     orthoAxes[i].SetInvViewMat(axis.GetMatrix());
        // }

        bool clearMipmap = (CVarVoxelClear.Get() & 8) == 8;
        // auto drawID = scene.GenerateDrawsForView(graph, ortho.visibilityMask, 3);

        auto voxelGridExtents = vk::Extent3D(voxelGridSize.x, voxelGridSize.y, voxelGridSize.z);
        // auto voxelGridMips = CalculateMipmapLevels(voxelGridExtents);

        voxelLayerCount = CVarVoxelLayers.Get();
        fragmentListCount = std::min(MAX_VOXEL_FRAGMENT_LISTS, CVarVoxelFragmentBuckets.Get());

        uint32 totalFragmentListSize = 0;
        {
            auto fragmentListSize = (voxelGridSize.x * voxelGridSize.y * voxelGridSize.z) / 2;
            for (uint32 i = 0; i < fragmentListCount; i++) {
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
                desc.sampler = SamplerType::TrilinearClampBorder;
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                for (auto &voxelLayer : VoxelLayers) {
                    if (voxelLayer.layerIndex >= voxelLayerCount) continue;
                    builder.CreateImage(voxelLayer.name,
                        desc,
                        voxelLayer.layerIndex == 0 && clearMipmap ? Access::TransferWrite : Access::None);
                    builder.CreateImage(voxelLayer.preBlurName,
                        desc,
                        voxelLayer.layerIndex == 0 && clearMipmap ? Access::TransferWrite : Access::None);
                }
            })
            .Execute([this, clearMipmap](rg::Resources &resources, CommandContext &cmd) {
                if (clearMipmap) {
                    vk::ClearColorValue clear;
                    vk::ImageSubresourceRange range;
                    range.layerCount = 1;
                    range.levelCount = 1;
                    range.aspectMask = vk::ImageAspectFlagBits::eColor;

                    for (auto &voxelLayer : VoxelLayers) {
                        if (voxelLayer.layerIndex >= 1) continue;
                        auto layerView = resources.GetImageView(voxelLayer.name);
                        cmd.Raw().clearColorImage(*layerView->Image(),
                            vk::ImageLayout::eTransferDstOptimal,
                            clear,
                            {range});

                        auto layerView2 = resources.GetImageView(voxelLayer.preBlurName);
                        cmd.Raw().clearColorImage(*layerView2->Image(),
                            vk::ImageLayout::eTransferDstOptimal,
                            clear,
                            {range});
                    }
                }
            });

        struct GPULayerData {
            glm::vec3 direction;
            uint32_t layerIndex;
        };
        static_assert(sizeof(GPULayerData) == sizeof(glm::vec4), "GPULayerData size missmatch");

        for (auto &voxelLayer : VoxelLayers) {
            if (voxelLayer.layerIndex >= voxelLayerCount) continue;
            if (voxelLayer.layerIndex == 0) {
                graph.AddPass(voxelLayer.preBlurName)
                    .Build([&](rg::PassBuilder &builder) {
                        builder.Write(voxelLayer.preBlurName, Access::ComputeShaderWrite);

                        builder.ReadUniform("VoxelState");

                        builder.Read("Voxels.FragmentListMetadata", Access::IndirectBuffer);
                        builder.Read("Voxels.FragmentListMetadata", Access::ComputeShaderReadStorage);
                        builder.Read("Voxels.FragmentLists", Access::ComputeShaderReadStorage);
                    })

                    .Execute([this, voxelLayer](rg::Resources &resources, CommandContext &cmd) {
                        cmd.SetComputeShader("voxel_fill_layer.comp");

                        cmd.SetUniformBuffer(0, 0, resources.GetBuffer("VoxelState"));

                        GPULayerData layerData = {
                            directions[voxelLayer.dirIndex],
                            voxelLayer.layerIndex,
                        };
                        cmd.UploadUniformData(0, 1, &layerData);

                        cmd.SetImageView(0, 2, resources.GetImageView(voxelLayer.preBlurName));

                        auto metadata = resources.GetBuffer("Voxels.FragmentListMetadata");
                        cmd.SetStorageBuffer(0, 3, metadata, 0, sizeof(GPUVoxelFragmentList));

                        cmd.SetStorageBuffer(0,
                            4,
                            resources.GetBuffer("Voxels.FragmentLists"),
                            fragmentListSizes[0].offset * sizeof(GPUVoxelFragment),
                            fragmentListSizes[0].capacity * sizeof(GPUVoxelFragment));

                        cmd.DispatchIndirect(metadata, offsetof(GPUVoxelFragmentList, cmd));
                    });

                for (uint32_t i = 1; i < fragmentListCount; i++) {
                    graph.AddPass("Merge")
                        .Build([&](rg::PassBuilder &builder) {
                            builder.Read(voxelLayer.preBlurName, Access::ComputeShaderReadStorage);
                            builder.Write(voxelLayer.preBlurName, Access::ComputeShaderWrite);

                            builder.ReadUniform("VoxelState");

                            builder.Read("Voxels.FragmentListMetadata", Access::IndirectBuffer);
                            builder.Read("Voxels.FragmentListMetadata", Access::ComputeShaderReadStorage);
                            builder.Read("Voxels.FragmentLists", Access::ComputeShaderReadStorage);
                        })
                        .Execute([this, i, voxelLayer](rg::Resources &resources, CommandContext &cmd) {
                            cmd.SetComputeShader("voxel_merge_layer.comp");
                            cmd.SetShaderConstant(ShaderStage::Compute, 0, i);

                            cmd.SetUniformBuffer(0, 0, resources.GetBuffer("VoxelState"));

                            GPULayerData layerData = {
                                directions[voxelLayer.dirIndex],
                                voxelLayer.layerIndex,
                            };
                            cmd.UploadUniformData(0, 1, &layerData);

                            cmd.SetImageView(0, 2, resources.GetImageView(voxelLayer.preBlurName));

                            auto metadata = resources.GetBuffer("Voxels.FragmentListMetadata");
                            cmd.SetStorageBuffer(0,
                                3,
                                metadata,
                                i * sizeof(GPUVoxelFragmentList),
                                sizeof(GPUVoxelFragmentList));

                            cmd.SetStorageBuffer(0,
                                4,
                                resources.GetBuffer("Voxels.FragmentLists"),
                                fragmentListSizes[i].offset * sizeof(GPUVoxelFragment),
                                fragmentListSizes[i].capacity * sizeof(GPUVoxelFragment));

                            cmd.DispatchIndirect(metadata,
                                i * sizeof(GPUVoxelFragmentList) + offsetof(GPUVoxelFragmentList, cmd));
                        });
                }
            } else {
                graph.AddPass(voxelLayer.preBlurName)
                    .Build([&](rg::PassBuilder &builder) {
                        builder.Write(voxelLayer.preBlurName, Access::ComputeShaderWrite);

                        for (auto &voxelLayer2 : VoxelLayers) {
                            if (voxelLayer2.layerIndex + 1 == voxelLayer.layerIndex &&
                                voxelLayer2.dirIndex == voxelLayer.dirIndex) {
                                builder.Read(voxelLayer2.name, Access::ComputeShaderSampleImage);
                                break;
                            }
                        }
                    })
                    .Execute([this, voxelLayer](rg::Resources &resources, CommandContext &cmd) {
                        cmd.SetComputeShader("voxel_mipmap_layer.comp");

                        GPULayerData layerData = {
                            directions[voxelLayer.dirIndex],
                            voxelLayer.layerIndex,
                        };
                        cmd.UploadUniformData(0, 0, &layerData);

                        for (auto &voxelLayer2 : VoxelLayers) {
                            if (voxelLayer2.layerIndex + 1 == voxelLayer.layerIndex &&
                                voxelLayer2.dirIndex == voxelLayer.dirIndex) {
                                cmd.SetImageView(0, 1, resources.GetImageView(voxelLayer2.name));
                                break;
                            }
                        }

                        cmd.SetImageView(0, 2, resources.GetImageView(voxelLayer.preBlurName));

                        auto dispatchCount = (voxelGridSize + 7) / 8;
                        cmd.Dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
                    });
            }

            graph.AddPass("BlurLayer")
                .Build([&](rg::PassBuilder &builder) {
                    builder.Read(voxelLayer.preBlurName, Access::ComputeShaderSampleImage);
                    builder.Write(voxelLayer.name, Access::ComputeShaderWrite);
                })
                .Execute([this, voxelLayer](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetComputeShader("voxel_mipmap_layer_blur.comp");

                    GPULayerData layerData = {
                        directions[voxelLayer.dirIndex],
                        voxelLayer.layerIndex,
                    };
                    cmd.UploadUniformData(0, 0, &layerData);

                    cmd.SetImageView(0, 1, resources.GetImageView(voxelLayer.preBlurName));
                    cmd.SetImageView(0, 2, resources.GetImageView(voxelLayer.name));

                    auto dispatchCount = (voxelGridSize + 7) / 8;
                    cmd.Dispatch(dispatchCount.x, dispatchCount.y, dispatchCount.z);
                });
        }
    }

    void Voxels::AddDebugPass(RenderGraph &graph) {
        if (CVarVoxelDebug.Get() <= 0 || voxelGridSize == glm::ivec3(0)) return;
        auto debugMipLayer = std::min(voxelLayerCount - 1, CVarVoxelDebugMip.Get());

        graph.AddPass("VoxelDebug")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("Voxels.FillCounters", Access::FragmentShaderReadStorage);
                builder.Read("Voxels.Radiance", Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Normals", Access::FragmentShaderSampleImage);
                builder.ReadUniform("ViewState");
                builder.ReadUniform("VoxelState");
                builder.Read("ExposureState", Access::FragmentShaderReadStorage);

                for (auto &voxelLayer : VoxelLayers) {
                    if (voxelLayer.layerIndex != debugMipLayer) continue;
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

                cmd.SetUniformBuffer(0, 0, resources.GetBuffer("ViewState"));
                cmd.SetUniformBuffer(0, 1, resources.GetBuffer("VoxelState"));
                cmd.SetStorageBuffer(0, 2, resources.GetBuffer("ExposureState"));
                cmd.SetImageView(0, 3, resources.GetImageView(resources.LastOutputID()));
                cmd.SetImageView(0, 4, resources.GetImageView("Voxels.FillCounters"));
                cmd.SetImageView(0, 5, resources.GetImageView("Voxels.Radiance"));
                cmd.SetImageView(0, 6, resources.GetImageView("Voxels.Normals"));

                for (auto &voxelLayer : VoxelLayers) {
                    if (voxelLayer.layerIndex != debugMipLayer) continue;
                    auto layerView = resources.GetImageView(voxelLayer.fullName);
                    Assertf(layerView, "Layer view missing: %s", voxelLayer.fullName);
                    cmd.SetImageView(1, voxelLayer.dirIndex, layerView);
                }

                cmd.SetShaderConstant(ShaderStage::Fragment, 0, CVarVoxelDebug.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, 1, CVarVoxelDebugBlend.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, 2, debugMipLayer);

                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
