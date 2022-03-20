#include "Lighting.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace sp::vulkan::renderer {
    static CVar<bool> CVarVSM("r.VSM", false, "Enable Variance Shadow Mapping");
    static CVar<int> CVarPCF("r.PCF", 1, "Enable screen space shadow filtering (0: off, 1: on, 2: shadow map blur");
    static CVar<int> CVarLightingMode("r.LightingMode",
        1,
        "Toggle between different lighting shader modes "
        "(0: direct only, 1: full lighting, 2: indirect only, 3: diffuse only, 4: specular only)");

    void Lighting::LoadState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::Light, ecs::TransformSnapshot>> lock) {
        lightCount = 0;
        shadowAtlasSize = glm::ivec2(0, 0);
        gelTextureCache.clear();

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

            data.gelId = 0;
            if (!light.gelName.empty()) {
                auto it = gelTextureCache.emplace(light.gelName, 0).first;
                gelTextures[lightCount] = std::make_pair(light.gelName, &it->second);
            } else {
                gelTextures[lightCount] = {};
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

        graph.AddPass("LightState")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateUniform("LightState", sizeof(gpuData));
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                resources.GetBuffer("LightState")->CopyFrom(&gpuData);
            });
    }

    void Lighting::AddGelTextures(RenderGraph &graph) {
        graph.AddPass("GelTextures")
            .Build([&](rg::PassBuilder &builder) {
                for (auto &gel : gelTextureCache) {
                    builder.Read(gel.first, Access::FragmentShaderSampleImage);
                }
                builder.Write("LightState", Access::HostWrite);
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                for (auto &gel : gelTextureCache) {
                    gel.second = scene.textures.Add(resources.GetImageView(gel.first)).index;
                }
                for (size_t i = 0; i < MAX_LIGHTS; i++) {
                    if (gelTextures[i].second) gpuData.lights[i].gelId = *gelTextures[i].second;
                }
                resources.GetBuffer("LightState")->CopyFrom(&gpuData);
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
                ImageDesc desc;
                auto extent = glm::max(glm::ivec2(1), shadowAtlasSize);
                desc.extent = vk::Extent3D(extent.x, extent.y, 1);

                desc.format = CVarVSM.Get() ? vk::Format::eR32G32Sfloat : vk::Format::eR32Sfloat;
                builder.OutputColorAttachment(0, "ShadowMapLinear", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eD16Unorm;
                builder.OutputDepthAttachment("ShadowMapDepth", desc, {LoadOp::Clear, StoreOp::Store});

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);

                for (auto &ids : drawIDs) {
                    builder.Read(ids.drawCommandsBuffer, Access::IndirectBuffer);
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
        auto depthTarget = (CVarVSM.Get() || CVarPCF.Get() == 2) ? "ShadowMapBlur.LastOutput" : "ShadowMapLinear";

        graph.AddPass("Lighting")
            .Build([&](rg::PassBuilder &builder) {
                auto gBuffer0 = builder.Read("GBuffer0", Access::FragmentShaderSampleImage);
                builder.Read("GBuffer1", Access::FragmentShaderSampleImage);
                builder.Read("GBuffer2", Access::FragmentShaderSampleImage);
                builder.Read(depthTarget, Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Radiance", Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveImage(gBuffer0);
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(0, "LinearLuminance", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ReadUniform("VoxelState");
                builder.Read("ExposureState", Access::FragmentShaderReadStorage);
                builder.ReadUniform("ViewState");
                builder.ReadUniform("LightState");

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});
            })
            .Execute([this, depthTarget](rg::Resources &resources, CommandContext &cmd) {
                if (CVarVSM.Get()) {
                    cmd.SetShaders("screen_cover.vert", "lighting_vsm.frag");
                } else if (CVarPCF.Get() > 0) {
                    cmd.SetShaders("screen_cover.vert", "lighting_pcf.frag");
                } else {
                    cmd.SetShaders("screen_cover.vert", "lighting.frag");
                }

                cmd.SetStencilTest(true);
                cmd.SetDepthTest(false, false);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetImageView(0, 0, resources.GetImageView("GBuffer0"));
                cmd.SetImageView(0, 1, resources.GetImageView("GBuffer1"));
                cmd.SetImageView(0, 2, resources.GetImageView("GBuffer2"));
                cmd.SetImageView(0, 3, resources.GetImageView(depthTarget));
                cmd.SetImageView(0, 4, resources.GetImageView("Voxels.Radiance"));

                cmd.SetBindlessDescriptors(1, scene.textures.GetDescriptorSet());

                cmd.SetUniformBuffer(0, 8, resources.GetBuffer("VoxelState"));
                cmd.SetStorageBuffer(0, 9, resources.GetBuffer("ExposureState"));
                cmd.SetUniformBuffer(0, 10, resources.GetBuffer("ViewState"));
                cmd.SetUniformBuffer(0, 11, resources.GetBuffer("LightState"));

                cmd.SetShaderConstant(ShaderStage::Fragment, 0, CVarLightingMode.Get());

                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
