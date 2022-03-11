#include "Voxels.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"

namespace sp::vulkan::renderer {
    static CVar<int> CVarVoxelDebug("r.VoxelDebug",
        0,
        "Enable voxel grid debug view (0: off, 1: ray march, 2: cone trace)");
    static CVar<float> CVarVoxelDebugBlend("r.VoxelDebugBlend", 0.0f, "The blend weight used to overlay voxel debug");
    static CVar<bool> CVarVoxelClear("r.VoxelClear", true, "Enable voxel grid clearing between frames");

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
                builder.UniformCreate("VoxelState", sizeof(GPUVoxelState));
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                GPUVoxelState gpuData = {glm::inverse(glm::mat4(voxelToWorld.matrix)), voxelGridSize};
                resources.GetBuffer("VoxelState")->CopyFrom(&gpuData);
            });
    }

    void Voxels::AddVoxelization(RenderGraph &graph) {
        if (voxelGridSize == glm::ivec3(0)) {
            graph.AddPass("DummyVoxelization")
                .Build([&](rg::PassBuilder &builder) {
                    RenderTargetDesc desc;
                    desc.extent = vk::Extent3D(1, 1, 1);
                    desc.primaryViewType = vk::ImageViewType::e3D;
                    desc.imageType = vk::ImageType::e3D;
                    desc.format = vk::Format::eR16G16B16A16Sfloat;
                    desc.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
                    builder.RenderTargetCreate("VoxelRadiance", desc);
                })
                .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                    auto radianceView = resources.GetRenderTarget("VoxelRadiance")->ImageView();

                    vk::ClearColorValue clear;
                    vk::ImageSubresourceRange range;
                    range.layerCount = 1;
                    range.levelCount = 1;
                    range.aspectMask = vk::ImageAspectFlagBits::eColor;
                    cmd.ImageBarrier(radianceView->Image(),
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eGeneral,
                        vk::PipelineStageFlagBits::eTopOfPipe,
                        {},
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::AccessFlagBits::eTransferWrite);
                    cmd.Raw().clearColorImage(*radianceView->Image(), vk::ImageLayout::eGeneral, clear, {range});
                });
            return;
        }

        struct GPUVoxelOverflowFragment {
            glm::vec3 position;
            glm::vec3 radiance;
        };

        struct GPUVoxelFragmentList {
            VkDispatchIndirectCommand fragmentCount;
            VkDispatchIndirectCommand overflowCounts[3];
            GPUVoxelOverflowFragment overflowBuckets[3][8096];
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
            axis.Translate(axis.GetRotationAndScale() * glm::vec3(0, 0, -0.5));
            orthoAxes[i].SetInvViewMat(axis.matrix);
        }

        auto drawID = scene.GenerateDrawsForView(graph, ortho.visibilityMask, 3);

        graph.AddPass("Voxelization")
            .Build([&](rg::PassBuilder &builder) {
                RenderTargetDesc desc;
                desc.extent = vk::Extent3D(voxelGridSize.x, voxelGridSize.y, voxelGridSize.z);
                desc.primaryViewType = vk::ImageViewType::e3D;
                desc.imageType = vk::ImageType::e3D;
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                desc.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
                builder.RenderTargetCreate("VoxelRadiance", desc);

                auto fragmentListId =
                    builder.StorageCreate("VoxelFragmentList", sizeof(GPUVoxelFragmentList), Residency::GPU_ONLY).id;
                builder.TransferWrite(fragmentListId);

                builder.UniformRead("VoxelState");
                builder.UniformRead("LightState");
                builder.TextureRead("ShadowMapLinear");

                builder.BufferAccess("WarpedVertexBuffer",
                    rg::PipelineStage::eVertexInput,
                    rg::Access::eVertexAttributeRead,
                    rg::BufferUsage::eVertexBuffer);
                builder.BufferAccess(drawID.drawCommandsBuffer,
                    rg::PipelineStage::eDrawIndirect,
                    rg::Access::eIndirectCommandRead,
                    rg::BufferUsage::eIndirectBuffer);
                builder.StorageRead(drawID.drawParamsBuffer, rg::PipelineStage::eVertexShader);
            })

            .Execute([this, drawID, orthoAxes](rg::Resources &resources, CommandContext &cmd) {
                auto radianceView = resources.GetRenderTarget("VoxelRadiance")->ImageView();

                if (CVarVoxelClear.Get()) {
                    vk::ClearColorValue clear;
                    vk::ImageSubresourceRange range;
                    range.layerCount = 1;
                    range.levelCount = 1;
                    range.aspectMask = vk::ImageAspectFlagBits::eColor;
                    cmd.ImageBarrier(radianceView->Image(),
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eGeneral,
                        vk::PipelineStageFlagBits::eTopOfPipe,
                        {},
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::AccessFlagBits::eTransferWrite);
                    cmd.Raw().clearColorImage(*radianceView->Image(), vk::ImageLayout::eGeneral, clear, {range});
                    cmd.ImageBarrier(radianceView->Image(),
                        vk::ImageLayout::eGeneral,
                        vk::ImageLayout::eGeneral,
                        vk::PipelineStageFlagBits::eTransfer,
                        vk::AccessFlagBits::eTransferWrite,
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::AccessFlagBits::eShaderWrite);
                } else {
                    cmd.ImageBarrier(radianceView->Image(),
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eGeneral,
                        vk::PipelineStageFlagBits::eTopOfPipe,
                        {},
                        vk::PipelineStageFlagBits::eFragmentShader,
                        vk::AccessFlagBits::eShaderWrite);
                }

                auto fragmentListBuffer = resources.GetBuffer("VoxelFragmentList");
                cmd.Raw().fillBuffer(*fragmentListBuffer, 0, sizeof(GPUVoxelFragmentList), 0);

                RenderTargetDesc desc;
                desc.extent = vk::Extent3D(std::max(voxelGridSize.x, voxelGridSize.z),
                    std::max(voxelGridSize.y, voxelGridSize.z),
                    1);
                desc.format = vk::Format::eR8Sint;
                desc.usage = vk::ImageUsageFlagBits::eColorAttachment;
                auto dummyTarget = cmd.Device().GetRenderTarget(desc);

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
                cmd.SetImageView(0, 3, resources.GetRenderTarget("ShadowMapLinear")->ImageView());
                cmd.SetImageView(0, 4, radianceView);

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawID.drawCommandsBuffer),
                    resources.GetBuffer(drawID.drawParamsBuffer));

                cmd.EndRenderPass();
            });
    }

    void Voxels::AddDebugPass(RenderGraph &graph) {
        if (CVarVoxelDebug.Get() <= 0) return;

        graph.AddPass("VoxelDebug")
            .Build([&](rg::PassBuilder &builder) {
                builder.TextureRead("VoxelRadiance");
                builder.UniformRead("ViewState");
                builder.UniformRead("VoxelState");
                builder.StorageRead("ExposureState");

                auto lastOutput = builder.TextureRead(builder.LastOutputID());

                auto desc = lastOutput.DeriveRenderTarget();
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
                cmd.SetImageView(0, 3, resources.GetRenderTarget("VoxelRadiance")->ImageView());
                cmd.SetImageView(0, 4, resources.GetRenderTarget(resources.LastOutputID())->ImageView());

                cmd.SetShaderConstant(ShaderStage::Fragment, 0, CVarVoxelDebugBlend.Get());

                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
