#include "Voxels.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"
#include "graphics/vulkan/render_passes/Lighting.hh"

namespace sp::vulkan::renderer {
    static CVar<int> CVarVoxelDebug("r.VoxelDebug",
        0,
        "Enable voxel grid debug view (0: off, 1: ray march, 2: cone trace)");
    static CVar<float> CVarVoxelDebugBlend("r.VoxelDebugBlend", 0.0f, "The blend weight used to overlay voxel debug");
    static CVar<int> CVarVoxelClear("r.VoxelClear",
        3,
        "Change the voxel grid clearing operation used between frames "
        "(0: no clear, 1: clear radiance, 2: clear counters: 3: clear all)");

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

        struct GPUVoxelState {
            glm::mat4 worldToVoxel;
            glm::ivec3 size;
        };

        graph.AddPass("VoxelState")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateUniform("VoxelState", sizeof(GPUVoxelState));
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                GPUVoxelState gpuData = {glm::inverse(glm::mat4(voxelToWorld.matrix)), voxelGridSize};
                resources.GetBuffer("VoxelState")->CopyFrom(&gpuData);
            });
    }

    void Voxels::AddVoxelization(RenderGraph &graph, const Lighting &lighting) {
        auto scope = graph.Scope("Voxels");

        if (voxelGridSize == glm::ivec3(0)) {
            graph.AddPass("Dummy")
                .Build([&](rg::PassBuilder &builder) {
                    ImageDesc desc;
                    desc.extent = vk::Extent3D(1, 1, 1);
                    desc.primaryViewType = vk::ImageViewType::e3D;
                    desc.imageType = vk::ImageType::e3D;
                    desc.format = vk::Format::eR16G16B16A16Sfloat;
                    builder.CreateImage("Radiance", desc, Access::TransferWrite);
                })
                .Execute([this](rg::Resources &resources, CommandContext &cmd) {
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
                });
            return;
        }

        struct GPUVoxelFragment {
            uint16_t position[3];
            uint16_t radiance[3]; // half-float formatted
        };

        // Separate buffers store lists as GPUVoxelFragment[count]
        struct GPUVoxelFragmentCounts {
            uint32_t fragmentCount;
            uint32_t overflowCount[3];
            VkDispatchIndirectCommand fragmentsCmd;
            VkDispatchIndirectCommand overflowCmd[3];
        };

        ecs::View ortho;
        ortho.visibilityMask.set(ecs::Renderable::VISIBLE_LIGHTING_VOXEL);

        auto voxelCenter = voxelToWorld;
        voxelCenter.Translate(glm::mat3(voxelCenter.matrix) * (0.5f * glm::vec3(voxelGridSize)));

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
            axis.Translate(glm::mat3(axis.matrix) * glm::vec3(0, 0, -0.5));
            orthoAxes[i].SetInvViewMat(axis.matrix);
        }

        bool clearRadiance = (CVarVoxelClear.Get() & 1) == 1;
        bool clearCounters = (CVarVoxelClear.Get() & 2) == 2;
        auto fragmentListSize = (voxelGridSize.x * voxelGridSize.y * voxelGridSize.z) / 2;
        auto drawID = scene.GenerateDrawsForView(graph, ortho.visibilityMask, 3);

        graph.AddPass("Init")
            .Build([&](rg::PassBuilder &builder) {
                ImageDesc desc;
                desc.extent = vk::Extent3D(voxelGridSize.x, voxelGridSize.y, voxelGridSize.z);
                desc.primaryViewType = vk::ImageViewType::e3D;
                desc.imageType = vk::ImageType::e3D;

                desc.format = vk::Format::eR32Uint;
                builder.CreateImage("FillCounters",
                    desc,
                    clearCounters ? Access::TransferWrite : Access::FragmentShaderWrite);

                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.CreateImage("Radiance",
                    desc,
                    clearRadiance ? Access::TransferWrite : Access::FragmentShaderWrite);

                builder.CreateBuffer("FragmentCounts",
                    sizeof(GPUVoxelFragmentCounts),
                    Residency::GPU_ONLY,
                    Access::TransferWrite);

                builder.CreateBuffer("FragmentList",
                    sizeof(GPUVoxelFragment) * fragmentListSize,
                    Residency::GPU_ONLY,
                    Access::FragmentShaderWrite);
                builder.CreateBuffer("OverflowList0",
                    sizeof(GPUVoxelFragment) * fragmentListSize,
                    Residency::GPU_ONLY,
                    Access::FragmentShaderWrite);
                builder.CreateBuffer("OverflowList1",
                    sizeof(GPUVoxelFragment) * fragmentListSize,
                    Residency::GPU_ONLY,
                    Access::FragmentShaderWrite);
                builder.CreateBuffer("OverflowList2",
                    sizeof(GPUVoxelFragment) * fragmentListSize,
                    Residency::GPU_ONLY,
                    Access::FragmentShaderWrite);
            })
            .Execute(
                [this, drawID, orthoAxes, clearRadiance, clearCounters](rg::Resources &resources, CommandContext &cmd) {
                    if (clearRadiance) {
                        vk::ClearColorValue clear;
                        vk::ImageSubresourceRange range;
                        range.layerCount = 1;
                        range.levelCount = 1;
                        range.aspectMask = vk::ImageAspectFlagBits::eColor;
                        auto radianceView = resources.GetImageView("Radiance");
                        cmd.Raw().clearColorImage(*radianceView->Image(),
                            vk::ImageLayout::eTransferDstOptimal,
                            clear,
                            {range});
                    }
                    if (clearCounters) {
                        vk::ClearColorValue clear;
                        vk::ImageSubresourceRange range;
                        range.layerCount = 1;
                        range.levelCount = 1;
                        range.aspectMask = vk::ImageAspectFlagBits::eColor;
                        auto counterView = resources.GetImageView("FillCounters");
                        cmd.Raw().clearColorImage(*counterView->Image(),
                            vk::ImageLayout::eTransferDstOptimal,
                            clear,
                            {range});
                    }

                    auto countsBuffer = resources.GetBuffer("FragmentCounts");
                    cmd.Raw().fillBuffer(*countsBuffer, 0, sizeof(GPUVoxelFragmentCounts), 0);
                    cmd.Raw().fillBuffer(*countsBuffer,
                        offsetof(GPUVoxelFragmentCounts, fragmentsCmd.y),
                        sizeof(uint32_t) * 2,
                        1);
                    cmd.Raw().fillBuffer(*countsBuffer,
                        offsetof(GPUVoxelFragmentCounts, overflowCmd[0].y),
                        sizeof(uint32_t) * 2,
                        1);
                    cmd.Raw().fillBuffer(*countsBuffer,
                        offsetof(GPUVoxelFragmentCounts, overflowCmd[1].y),
                        sizeof(uint32_t) * 2,
                        1);
                    cmd.Raw().fillBuffer(*countsBuffer,
                        offsetof(GPUVoxelFragmentCounts, overflowCmd[2].y),
                        sizeof(uint32_t) * 2,
                        1);
                });

        graph.AddPass("Fill")
            .Build([&](rg::PassBuilder &builder) {
                builder.Write("FillCounters", Access::FragmentShaderWrite);
                builder.Write("Radiance", Access::FragmentShaderWrite);

                builder.ReadUniform("VoxelState");
                builder.ReadUniform("LightState");
                builder.Read("ShadowMapLinear", Access::FragmentShaderSampleImage);

                builder.Write("FragmentCounts", Access::FragmentShaderWrite);
                builder.Write("FragmentList", Access::FragmentShaderWrite);
                builder.Write("OverflowList0", Access::FragmentShaderWrite);
                builder.Write("OverflowList1", Access::FragmentShaderWrite);
                builder.Write("OverflowList2", Access::FragmentShaderWrite);

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawID.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawID.drawParamsBuffer, Access::VertexShaderReadStorage);
            })

            .Execute([this, drawID, orthoAxes, &lighting](rg::Resources &resources, CommandContext &cmd) {
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
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::AccessFlagBits::eShaderWrite);

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
                cmd.SetImageView(0, 3, resources.GetImageView("ShadowMapLinear"));
                cmd.SetImageView(0, 4, resources.GetImageView("FillCounters"));
                cmd.SetImageView(0, 5, resources.GetImageView("Radiance"));

                cmd.SetStorageBuffer(3, 0, resources.GetBuffer("FragmentCounts"));
                cmd.SetStorageBuffer(3, 1, resources.GetBuffer("FragmentList"));
                cmd.SetStorageBuffer(3, 2, resources.GetBuffer("OverflowList0"));
                cmd.SetStorageBuffer(3, 3, resources.GetBuffer("OverflowList1"));
                cmd.SetStorageBuffer(3, 4, resources.GetBuffer("OverflowList2"));

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawID.drawCommandsBuffer),
                    resources.GetBuffer(drawID.drawParamsBuffer));

                cmd.EndRenderPass();
            });

        static const char *passNames[3] = {"Merge0", "Merge1", "Merge2"};
        for (uint32_t i = 0; i < 3; i++) {
            graph.AddPass(passNames[i])
                .Build([&](rg::PassBuilder &builder) {
                    builder.Read("FillCounters", Access::ComputeShaderSampleImage);
                    builder.Write("Radiance", Access::ComputeShaderWrite);

                    builder.Read("FragmentCounts", Access::IndirectBuffer);
                    builder.Read("FragmentList", Access::ComputeShaderReadStorage);
                    builder.Read("OverflowList0", Access::ComputeShaderReadStorage);
                    builder.Read("OverflowList1", Access::ComputeShaderReadStorage);
                    builder.Read("OverflowList2", Access::ComputeShaderReadStorage);
                })
                .Execute([i](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetComputeShader("voxel_merge.comp");

                    cmd.SetImageView(0, 0, resources.GetImageView("FillCounters"));
                    cmd.SetImageView(0, 1, resources.GetImageView("Radiance"));

                    cmd.SetStorageBuffer(1, 0, resources.GetBuffer("FragmentCounts"));
                    cmd.SetStorageBuffer(1, 1, resources.GetBuffer("FragmentList"));
                    cmd.SetStorageBuffer(1, 2, resources.GetBuffer("OverflowList0"));
                    cmd.SetStorageBuffer(1, 3, resources.GetBuffer("OverflowList1"));
                    cmd.SetStorageBuffer(1, 4, resources.GetBuffer("OverflowList2"));

                    cmd.SetShaderConstant(ShaderStage::Compute, 0, i);

                    cmd.DispatchIndirect(resources.GetBuffer("FragmentCounts"),
                        offsetof(GPUVoxelFragmentCounts, overflowCmd) +
                            i * sizeof(GPUVoxelFragmentCounts::overflowCmd[0]));
                });
        }
    }

    void Voxels::AddDebugPass(RenderGraph &graph) {
        if (CVarVoxelDebug.Get() <= 0 || voxelGridSize == glm::ivec3(0)) return;

        graph.AddPass("VoxelDebug")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read("Voxels.FillCounters", Access::FragmentShaderReadStorage);
                builder.Read("Voxels.Radiance", Access::FragmentShaderReadStorage);
                builder.ReadUniform("ViewState");
                builder.ReadUniform("VoxelState");
                builder.Read("ExposureState", Access::FragmentShaderReadStorage);

                builder.Read(builder.LastOutputID(), Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveImage(builder.LastOutputID());
                builder.OutputColorAttachment(0, "VoxelDebug", desc, {LoadOp::DontCare, StoreOp::Store});
                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});
            })
            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "voxel_debug.frag");
                cmd.SetStencilTest(true);
                cmd.SetDepthTest(false, false);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetUniformBuffer(0, 0, resources.GetBuffer("ViewState"));
                cmd.SetUniformBuffer(0, 1, resources.GetBuffer("VoxelState"));
                cmd.SetStorageBuffer(0, 2, resources.GetBuffer("ExposureState"));
                cmd.SetImageView(0, 3, resources.GetImageView("Voxels.FillCounters"));
                cmd.SetImageView(0, 4, resources.GetImageView("Voxels.Radiance"));
                cmd.SetImageView(0, 5, resources.GetImageView(resources.LastOutputID()));

                cmd.SetShaderConstant(ShaderStage::Fragment, 0, CVarVoxelDebugBlend.Get());

                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
