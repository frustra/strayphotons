#include "Voxels.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"

namespace sp::vulkan::renderer {
    static CVar<int> CVarDebugVoxels("r.DebugVoxels",
        0,
        "Enable voxel grid debug view (0: off, 1: ray march, 2: cone trace)");

    void Voxels::LoadState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::VoxelArea, ecs::TransformSnapshot>> lock) {
        for (auto entity : lock.EntitiesWith<ecs::VoxelArea>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &area = entity.Get<ecs::VoxelArea>(lock);
            if (area.extents.x > 0 && area.extents.y > 0 && area.extents.z > 0) {
                voxelGridSize = area.extents;
                voxelGridOrigin = entity.Get<ecs::TransformSnapshot>(lock);
                break; // Only 1 voxel area supported for now
            }
        }
        gpuData.origin = glm::inverse(glm::mat4(voxelGridOrigin.matrix));
        gpuData.size = voxelGridSize;

        graph.AddPass("VoxelState")
            .Build([&](rg::PassBuilder &builder) {
                builder.UniformCreate("VoxelState", sizeof(gpuData));
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                resources.GetBuffer("VoxelState")->CopyFrom(&gpuData);
            });
    }

    void Voxels::AddVoxelization(RenderGraph &graph) {
        ecs::View ortho;
        ortho.visibilityMask.set(ecs::Renderable::VISIBLE_LIGHTING_VOXEL);
        auto voxelScale = voxelGridOrigin.GetScale();
        auto voxelMat = gpuData.origin;
        voxelMat[3] *= glm::vec4(2, 2, 1, 1) / glm::vec4(voxelGridSize, 1.0f);
        voxelMat = glm::translate(voxelMat, glm::vec3(-voxelScale.x, -voxelScale.y, 0));
        voxelMat = glm::scale(voxelMat, glm::vec3(2, 2, 1) / glm::vec3(voxelGridSize));
        ortho.SetViewMat(voxelMat);
        ortho.projMat = glm::identity<glm::mat4>();
        ortho.invProjMat = glm::identity<glm::mat4>();
        ortho.clearMode.reset();

        ecs::View orthoAxes[3] = {ortho, ortho, ortho};
        orthoAxes[0].extents = glm::ivec2(voxelGridSize.z, voxelGridSize.y);
        orthoAxes[1].extents = glm::ivec2(voxelGridSize.x, voxelGridSize.z);
        orthoAxes[2].extents = glm::ivec2(voxelGridSize.x, voxelGridSize.y);

        auto drawID = scene.GenerateDrawsForView(graph, ortho.visibilityMask);

        graph.AddPass("Voxelization")
            .Build([&](rg::PassBuilder &builder) {
                RenderTargetDesc desc;
                desc.extent = vk::Extent3D(voxelGridSize.x, voxelGridSize.y, voxelGridSize.z);
                desc.primaryViewType = vk::ImageViewType::e3D;
                desc.imageType = vk::ImageType::e3D;
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                desc.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
                builder.RenderTargetCreate("VoxelRadiance", desc);
                builder.UniformRead("VoxelState");

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
                    vk::PipelineStageFlagBits::eTopOfPipe,
                    {},
                    vk::PipelineStageFlagBits::eFragmentShader,
                    vk::AccessFlagBits::eShaderWrite);

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
                renderPass.PushColorAttachment(dummyTarget->ImageView(), LoadOp::DontCare, StoreOp::Store);
                cmd.BeginRenderPass(renderPass);

                cmd.SetShaders("voxel_fill.vert", "voxel_fill.frag");

                GPUViewState lightViews[] = {{orthoAxes[0]}, {orthoAxes[1]}, {orthoAxes[2]}};
                cmd.UploadUniformData(0, 0, lightViews, 3);

                vk::Rect2D viewport;
                viewport.extent = vk::Extent2D(std::max(voxelGridSize.x, voxelGridSize.z),
                    std::max(voxelGridSize.y, voxelGridSize.z));
                cmd.SetViewport(viewport);
                cmd.SetYDirection(YDirection::Down);
                cmd.SetDepthTest(false, false);
                cmd.SetCullMode(vk::CullModeFlagBits::eNone);

                cmd.SetUniformBuffer(0, 1, resources.GetBuffer("VoxelState"));
                cmd.SetImageView(0, 2, radianceView);

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawID.drawCommandsBuffer),
                    resources.GetBuffer(drawID.drawParamsBuffer));

                cmd.EndRenderPass();
            });
    }

    void Voxels::AddDebugPass(RenderGraph &graph) {
        if (CVarDebugVoxels.Get() <= 0) return;

        graph.AddPass("VoxelDebug")
            .Build([&](rg::PassBuilder &builder) {
                builder.TextureRead("VoxelRadiance");
                builder.UniformRead("ViewState");
                builder.UniformRead("VoxelState");

                auto desc = builder.LastOutput().DeriveRenderTarget();
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
                cmd.SetImageView(0, 2, resources.GetRenderTarget("VoxelRadiance")->ImageView());

                cmd.SetYDirection(YDirection::Down);
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
