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
    static CVar<size_t> CVarVoxelFragmentBuckets("r.VoxelFragmentBuckets",
        4,
        "The number of fragments that can be written to a voxel.");

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
            float _padding[1];
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
                .Execute([](rg::Resources &resources, CommandContext &cmd) {
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

        struct GPUVoxelFragmentList {
            uint32_t count;
            uint32_t capacity;
            VkDispatchIndirectCommand cmd;
            // GPUVoxelFragment list[];
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

        fragmentListBuffers.clear();

        graph.AddPass("Init")
            .Build([&](rg::PassBuilder &builder) {
                ImageDesc desc;
                desc.extent = vk::Extent3D(voxelGridSize.x, voxelGridSize.y, voxelGridSize.z);
                desc.primaryViewType = vk::ImageViewType::e3D;
                desc.imageType = vk::ImageType::e3D;

                desc.format = vk::Format::eR32Uint;
                builder.CreateImage("FillCounters", desc, clearCounters ? Access::TransferWrite : Access::None);

                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.CreateImage("Radiance", desc, clearRadiance ? Access::TransferWrite : Access::None);

                for (size_t i = 0; i < CVarVoxelFragmentBuckets.Get(); i++) {
                    auto buf = builder.CreateBuffer(
                        sizeof(GPUVoxelFragmentList) + sizeof(GPUVoxelFragment) * fragmentListSize,
                        Residency::GPU_ONLY,
                        Access::TransferWrite);
                    fragmentListBuffers.emplace_back(buf.id);
                }
            })
            .Execute([this, clearRadiance, clearCounters, fragmentListSize](rg::Resources &resources,
                         CommandContext &cmd) {
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

                for (auto &resourceId : fragmentListBuffers) {
                    auto listBuffer = resources.GetBuffer(resourceId);
                    cmd.Raw().fillBuffer(*listBuffer, 0, sizeof(GPUVoxelFragmentList), 0);
                    cmd.Raw().fillBuffer(*listBuffer,
                        offsetof(GPUVoxelFragmentList, capacity),
                        sizeof(uint32_t),
                        fragmentListSize);
                    cmd.Raw().fillBuffer(*listBuffer, offsetof(GPUVoxelFragmentList, cmd.y), sizeof(uint32_t) * 2, 1);
                }
            });

        graph.AddPass("Fill")
            .Build([&](rg::PassBuilder &builder) {
                builder.Write("FillCounters", Access::FragmentShaderWrite);
                builder.Write("Radiance", Access::FragmentShaderWrite);

                builder.ReadUniform("VoxelState");
                builder.ReadUniform("LightState");
                builder.Read("ShadowMapLinear", Access::FragmentShaderSampleImage);

                for (auto &resourceId : fragmentListBuffers) {
                    builder.Write(resourceId, Access::FragmentShaderWrite);
                }

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
                cmd.SetImageView(0, 3, resources.GetImageView("ShadowMapLinear"));
                cmd.SetImageView(0, 4, resources.GetImageView("FillCounters"));
                cmd.SetImageView(0, 5, resources.GetImageView("Radiance"));

                uint32 binding = 0;
                for (auto &resourceId : fragmentListBuffers) {
                    // TODO: somehow make this work like bindless?
                    cmd.SetStorageBuffer(3, binding++, resources.GetBuffer(resourceId));
                }

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawID.drawCommandsBuffer),
                    resources.GetBuffer(drawID.drawParamsBuffer));

                cmd.EndRenderPass();
            });

        for (uint32_t i = 1; i < fragmentListBuffers.size(); i++) {
            graph.AddPass("Merge")
                .Build([&](rg::PassBuilder &builder) {
                    builder.Read("FillCounters", Access::ComputeShaderSampleImage);
                    builder.Write("Radiance", Access::ComputeShaderWrite);

                    builder.Write(fragmentListBuffers[i], Access::IndirectBuffer);
                })
                .Execute([this, i](rg::Resources &resources, CommandContext &cmd) {
                    cmd.SetComputeShader("voxel_merge.comp");

                    cmd.SetImageView(0, 0, resources.GetImageView("FillCounters"));
                    cmd.SetImageView(0, 1, resources.GetImageView("Radiance"));

                    cmd.SetShaderConstant(ShaderStage::Compute, 0, i);

                    auto buffer = resources.GetBuffer(fragmentListBuffers[i]);
                    cmd.SetStorageBuffer(1, 0, buffer);

                    cmd.DispatchIndirect(buffer, offsetof(GPUVoxelFragmentList, cmd));
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
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
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
