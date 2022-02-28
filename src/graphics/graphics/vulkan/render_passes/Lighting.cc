#include "Lighting.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"

namespace sp::vulkan::renderer {
    static CVar<float> CVarExposure("r.Exposure", 1.0f, "Scale factor for linear luminosity buffer");
    static CVar<bool> CVarVSM("r.VSM", false, "Enable Variance Shadow Mapping");
    static CVar<bool> CVarPCF("r.PCF", true, "Enable screen space shadow filtering");

    void Lighting::LoadState(ecs::Lock<ecs::Read<ecs::Light, ecs::TransformSnapshot>> lock) {
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

                builder.ReadBuffer("WarpedVertexBuffer");

                for (auto &ids : drawIDs) {
                    builder.ReadBuffer(ids.drawCommandsBuffer);
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
        AddGaussianBlur1D(graph, blurY1, glm::ivec2(1, 0), 1);
        graph.EndScope();
    }

    void Lighting::AddLightingPass(RenderGraph &graph) {
        auto depthTarget = CVarVSM.Get() ? "ShadowMapBlur.LastOutput" : "ShadowMapLinear";

        graph.AddPass("Lighting")
            .Build([&](rg::PassBuilder &builder) {
                auto gBuffer0 = builder.ShaderRead("GBuffer0");
                builder.ShaderRead("GBuffer1");
                builder.ShaderRead("GBuffer2");
                builder.ShaderRead(depthTarget);

                auto desc = gBuffer0.DeriveRenderTarget();
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(0, "LinearLuminance", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ReadBuffer("ViewState");
                builder.CreateUniformBuffer("LightState", sizeof(gpuData));

                for (int i = 0; i < gelCount; i++) {
                    builder.ShaderRead(gelNames[i]);
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

                cmd.SetTexture(0, 0, resources.GetRenderTarget("GBuffer0")->ImageView());
                cmd.SetTexture(0, 1, resources.GetRenderTarget("GBuffer1")->ImageView());
                cmd.SetTexture(0, 2, resources.GetRenderTarget("GBuffer2")->ImageView());
                cmd.SetTexture(0, 3, resources.GetRenderTarget(depthTarget)->ImageView());

                cmd.PushConstants(CVarExposure.Get());

                for (int i = 0; i < MAX_LIGHT_GELS; i++) {
                    if (i < gelCount) {
                        const auto &target = resources.GetRenderTarget(gelNames[i]);
                        cmd.SetTexture(1, i, target->ImageView());
                    } else {
                        cmd.SetTexture(1, i, scene.textures.GetBlankPixel());
                    }
                }

                auto lightState = resources.GetBuffer("LightState");
                lightState->CopyFrom(&gpuData);

                cmd.SetUniformBuffer(0, 10, resources.GetBuffer("ViewState"));
                cmd.SetUniformBuffer(0, 11, lightState);
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
