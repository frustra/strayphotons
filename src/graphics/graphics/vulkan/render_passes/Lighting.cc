#include "Lighting.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"

namespace sp::vulkan::renderer {
    static CVar<bool> CVarVSM("r.VSM", false, "Enable Variance Shadow Mapping");
    static CVar<bool> CVarPCF("r.PCF", true, "Enable screen space shadow filtering");

    void Lighting::LoadState(ecs::Lock<ecs::Read<ecs::Light, ecs::VoxelArea, ecs::TransformSnapshot>> lock) {
        lightCount = 0;
        gelCount = 0;
        shadowAtlasSize = glm::ivec2(0, 0);

        for (auto entity : lock.EntitiesWith<ecs::Light>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &light = entity.Get<ecs::Light>(lock);
            if (!light.on) continue;

            int extent = (int)std::pow(2, light.shadowMapSize);

            auto &transform = entity.Get<ecs::TransformSnapshot>(lock);
            auto &view = views[lightCount];

            view.visibilityMask.set(ecs::Renderable::VISIBLE_LIGHTING_SHADOW);
            view.extents = {extent, extent};
            view.fov = light.spotAngle * 2.0f;
            view.offset = {shadowAtlasSize.x, 0};
            view.clearMode.reset();
            view.clip = light.shadowMapClip;
            view.UpdateProjectionMatrix();
            view.UpdateViewMatrix(lock, entity);

            auto &data = gpuData.lights[lightCount];
            data.position = transform.GetPosition();
            data.tint = light.tint;
            data.direction = transform.GetForward();
            data.spotAngleCos = cos(light.spotAngle);
            data.proj = view.projMat;
            data.invProj = view.invProjMat;
            data.view = view.viewMat;
            data.clip = view.clip;
            data.mapOffset = {shadowAtlasSize.x, 0, extent, extent};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;

            if (light.gelName.empty()) {
                data.gelId = 0;
            } else {
                Assert(gelCount < MAX_LIGHT_GELS, "too many light gels");
                data.gelId = ++gelCount;
                gelNames[data.gelId - 1] = light.gelName;
            }

            shadowAtlasSize.x += extent;
            if (extent > shadowAtlasSize.y) shadowAtlasSize.y = extent;
            if (++lightCount >= MAX_LIGHTS) break;
        }
        glm::vec4 mapOffsetScale(shadowAtlasSize, shadowAtlasSize);
        for (int i = 0; i < lightCount; i++) {
            gpuData.lights[i].mapOffset /= mapOffsetScale;
        }
        gpuData.count = lightCount;

        for (auto entity : lock.EntitiesWith<ecs::VoxelArea>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &area = entity.Get<ecs::VoxelArea>(lock);
            if (area.extents.x > 0 && area.extents.y > 0 && area.extents.z > 0) {
                voxelGridSize = area.extents;
                voxelGridOrigin = entity.Get<ecs::TransformSnapshot>(lock);
                break; // Only 1 voxel area supported for now
            }
        }
    }

    void Lighting::AddVoxelization(RenderGraph &graph) {
        ecs::View ortho;
        ortho.visibilityMask.set(ecs::Renderable::VISIBLE_LIGHTING_VOXEL);
        ortho.SetViewMat(glm::scale(glm::mat4(voxelGridOrigin.matrix), glm::vec3(voxelGridSize)));
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
                desc.usage = vk::ImageUsageFlagBits::eStorage;
                builder.RenderTargetCreate("VoxelRadiance", desc);

                builder.BufferAccess("WarpedVertexBuffer",
                    rg::PipelineStage::eVertexInput,
                    rg::Access::eVertexAttributeRead,
                    rg::BufferUsage::eVertexBuffer);

                builder.BufferAccess(drawID.drawCommandsBuffer,
                    rg::PipelineStage::eDrawIndirect,
                    rg::Access::eIndirectCommandRead,
                    rg::BufferUsage::eIndirectBuffer);
            })

            .Execute([this, drawID, orthoAxes](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("voxel_fill.vert", "voxel_fill.frag");

                GPUViewState lightViews[] = {{orthoAxes[0]}, {orthoAxes[1]}, {orthoAxes[2]}};
                cmd.UploadUniformData(0, 10, lightViews, 3);

                vk::Rect2D viewport;
                viewport.extent = vk::Extent2D(std::max(voxelGridSize.x, voxelGridSize.z),
                    std::max(voxelGridSize.y, voxelGridSize.z));
                cmd.SetViewport(viewport);
                cmd.SetYDirection(YDirection::Down);

                cmd.SetImageView(0, 0, resources.GetRenderTarget("VoxelRadiance")->ImageView());

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawID.drawCommandsBuffer),
                    {});
            });
    }

    void Lighting::AddShadowPasses(RenderGraph &graph) {
        vector<GPUScene::DrawBufferIDs> drawIDs;
        drawIDs.reserve(lightCount);
        for (int i = 0; i < lightCount; i++) {
            drawIDs.push_back(scene.GenerateDrawsForView(graph, views[i].visibilityMask));
        }

        graph.AddPass("ShadowMaps")
            .Build([&](rg::PassBuilder &builder) {
                RenderTargetDesc desc;
                auto extent = glm::max(glm::ivec2(1), shadowAtlasSize);
                desc.extent = vk::Extent3D(extent.x, extent.y, 1);

                desc.format = CVarVSM.Get() ? vk::Format::eR32G32Sfloat : vk::Format::eR32Sfloat;
                builder.OutputColorAttachment(0, "ShadowMapLinear", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eD16Unorm;
                builder.OutputDepthAttachment("ShadowMapDepth", desc, {LoadOp::Clear, StoreOp::Store});

                builder.BufferAccess("WarpedVertexBuffer",
                    rg::PipelineStage::eVertexInput,
                    rg::Access::eVertexAttributeRead,
                    rg::BufferUsage::eVertexBuffer);

                for (auto &ids : drawIDs) {
                    builder.BufferAccess(ids.drawCommandsBuffer,
                        rg::PipelineStage::eDrawIndirect,
                        rg::Access::eIndirectCommandRead,
                        rg::BufferUsage::eIndirectBuffer);
                }
            })

            .Execute([this, drawIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("shadow_map.vert", CVarVSM.Get() ? "shadow_map_vsm.frag" : "shadow_map.frag");

                for (int i = 0; i < lightCount; i++) {
                    auto &view = views[i];

                    GPUViewState lightViews[] = {{view}, {}};
                    cmd.UploadUniformData(0, 10, lightViews, 2);

                    vk::Rect2D viewport;
                    viewport.extent = vk::Extent2D(view.extents.x, view.extents.y);
                    viewport.offset = vk::Offset2D(view.offset.x, view.offset.y);
                    cmd.SetViewport(viewport);
                    cmd.SetYDirection(YDirection::Down);

                    auto &ids = drawIDs[i];
                    scene.DrawSceneIndirect(cmd,
                        resources.GetBuffer("WarpedVertexBuffer"),
                        resources.GetBuffer(ids.drawCommandsBuffer),
                        {});
                }
            });

        graph.BeginScope("ShadowMapBlur");
        auto sourceID = graph.LastOutputID();
        auto blurY1 = AddGaussianBlur1D(graph, sourceID, glm::ivec2(0, 1), 1);
        AddGaussianBlur1D(graph, blurY1, glm::ivec2(1, 0), 2);
        graph.EndScope();
    }

    void Lighting::AddLightingPass(RenderGraph &graph) {
        auto depthTarget = CVarVSM.Get() ? "ShadowMapBlur.LastOutput" : "ShadowMapLinear";

        graph.AddPass("Lighting")
            .Build([&](rg::PassBuilder &builder) {
                auto gBuffer0 = builder.TextureRead("GBuffer0");
                builder.TextureRead("GBuffer1");
                builder.TextureRead("GBuffer2");
                builder.TextureRead(depthTarget);

                builder.StorageRead("VoxelRadiance");

                auto desc = gBuffer0.DeriveRenderTarget();
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(0, "LinearLuminance", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.UniformRead("ViewState");
                builder.StorageRead("ExposureState");
                builder.UniformCreate("LightState", sizeof(gpuData));

                for (int i = 0; i < gelCount; i++) {
                    builder.TextureRead(gelNames[i]);
                }

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});
            })
            .Execute([this, depthTarget](rg::Resources &resources, CommandContext &cmd) {
                auto frag = CVarVSM.Get() ? "lighting_vsm.frag" : CVarPCF.Get() ? "lighting_pcf.frag" : "lighting.frag";
                cmd.SetShaders("screen_cover.vert", frag);
                cmd.SetStencilTest(true);
                cmd.SetDepthTest(false, false);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetImageView(0, 0, resources.GetRenderTarget("GBuffer0")->ImageView());
                cmd.SetImageView(0, 1, resources.GetRenderTarget("GBuffer1")->ImageView());
                cmd.SetImageView(0, 2, resources.GetRenderTarget("GBuffer2")->ImageView());
                cmd.SetImageView(0, 3, resources.GetRenderTarget(depthTarget)->ImageView());

                for (int i = 0; i < MAX_LIGHT_GELS; i++) {
                    if (i < gelCount) {
                        const auto &target = resources.GetRenderTarget(gelNames[i]);
                        cmd.SetImageView(1, i, target->ImageView());
                    } else {
                        cmd.SetImageView(1, i, scene.textures.GetBlankPixel());
                    }
                }

                auto lightState = resources.GetBuffer("LightState");
                lightState->CopyFrom(&gpuData);

                cmd.SetUniformBuffer(0, 10, resources.GetBuffer("ViewState"));
                cmd.SetUniformBuffer(0, 11, lightState);
                cmd.SetStorageBuffer(0, 9, resources.GetBuffer("ExposureState"));
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
