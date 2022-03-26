#include "Lighting.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"
#include "graphics/vulkan/render_passes/Readback.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

namespace sp::vulkan::renderer {
    static CVar<bool> CVarVSM("r.VSM", false, "Enable Variance Shadow Mapping");
    static CVar<int> CVarPCF("r.PCF", 1, "Enable screen space shadow filtering (0: off, 1: on, 2: shadow map blur");
    static CVar<int> CVarLightingMode("r.LightingMode",
        1,
        "Toggle between different lighting shader modes "
        "(0: direct only, 1: full lighting, 2: indirect only, 3: diffuse only, 4: specular only)");

    void Lighting::LoadState(RenderGraph &graph,
        ecs::Lock<ecs::Read<ecs::Name, ecs::Light, ecs::OpticalElement, ecs::TransformSnapshot>> lock) {
        lightCount = 0;
        shadowAtlasSize = glm::ivec2(0, 0);
        gelTextureCache.clear();
        lightEntities.clear();
        lightPaths.clear();

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

            lightEntities.emplace_back(entity);
            lightPaths.push_back({lightCount});

            shadowAtlasSize.x += extent;
            if (extent > shadowAtlasSize.y) shadowAtlasSize.y = extent;
            if (++lightCount >= MAX_LIGHTS) break;
        }
        glm::vec4 mapOffsetScale(shadowAtlasSize, shadowAtlasSize);
        for (uint32_t i = 0; i < lightCount; i++) {
            gpuData.lights[i].mapOffset /= mapOffsetScale;
        }
        gpuData.count = lightCount;

        for (auto &path : previousLightPaths) {
            std::string str;
            for (auto &e : path) {
                if (!str.empty()) str += ",";
                str += ecs::ToString(lock, e);
            }
            Logf("Visible path: %s", str);
        }

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
        vector<GPUScene::DrawBufferIDs> drawAllIDs, drawOpticIDs;
        drawAllIDs.reserve(lightCount);
        drawOpticIDs.reserve(lightCount);
        for (uint32_t i = 0; i < lightCount; i++) {
            drawAllIDs.push_back(scene.GenerateDrawsForView(graph, views[i].visibilityMask));

            auto opticMask = views[i].visibilityMask;
            opticMask.set(ecs::Renderable::VISIBLE_OPTICS);
            drawOpticIDs.push_back(scene.GenerateDrawsForView(graph, opticMask));
        }

        graph.AddPass("InitOptics")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateBuffer("OpticVisibility",
                    {sizeof(uint32_t), MAX_LIGHTS * MAX_OPTICS},
                    Residency::GPU_ONLY,
                    Access::TransferWrite);
            })
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
                auto visBuffer = resources.GetBuffer("OpticVisibility");
                cmd.Raw().fillBuffer(*visBuffer, 0, sizeof(uint32_t) * MAX_LIGHTS * MAX_OPTICS, 0);
            });

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

                for (auto &ids : drawAllIDs) {
                    builder.Read(ids.drawCommandsBuffer, Access::IndirectBuffer);
                    builder.Read(ids.drawParamsBuffer, Access::VertexShaderReadStorage);
                }
            })
            .Execute([this, drawAllIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("shadow_map.vert", CVarVSM.Get() ? "shadow_map_vsm.frag" : "shadow_map.frag");

                for (uint32_t i = 0; i < lightCount; i++) {
                    auto &view = views[i];

                    GPUViewState lightViews[] = {{view}, {}};
                    cmd.UploadUniformData(0, 0, lightViews, 2);

                    vk::Rect2D viewport;
                    viewport.extent = vk::Extent2D(view.extents.x, view.extents.y);
                    viewport.offset = vk::Offset2D(view.offset.x, view.offset.y);
                    cmd.SetViewport(viewport);
                    cmd.SetYDirection(YDirection::Down);

                    auto &ids = drawAllIDs[i];
                    scene.DrawSceneIndirect(cmd,
                        resources.GetBuffer("WarpedVertexBuffer"),
                        resources.GetBuffer(ids.drawCommandsBuffer),
                        resources.GetBuffer(ids.drawParamsBuffer));
                }
            });

        graph.AddPass("OpticsVisibility")
            .Build([&](rg::PassBuilder &builder) {
                builder.SetDepthAttachment("ShadowMapDepth", {LoadOp::Load, StoreOp::Store});

                builder.Write("OpticVisibility", Access::FragmentShaderWrite);
                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);

                for (auto &ids : drawOpticIDs) {
                    builder.Read(ids.drawCommandsBuffer, Access::IndirectBuffer);
                    builder.Read(ids.drawParamsBuffer, Access::VertexShaderReadStorage);
                }
            })
            .Execute([this, drawOpticIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("optic_visibility.vert", "optic_visibility.frag");

                struct {
                    uint32_t lightIndex;
                } constants;

                auto visBuffer = resources.GetBuffer("OpticVisibility");

                for (uint32_t i = 0; i < lightCount; i++) {
                    auto &view = views[i];

                    GPUViewState lightViews[] = {{view}, {}};
                    cmd.UploadUniformData(0, 0, lightViews, 2);
                    cmd.SetStorageBuffer(0, 1, visBuffer);

                    vk::Rect2D viewport;
                    viewport.extent = vk::Extent2D(view.extents.x, view.extents.y);
                    viewport.offset = vk::Offset2D(view.offset.x, view.offset.y);
                    cmd.SetViewport(viewport);
                    cmd.SetYDirection(YDirection::Down);
                    cmd.SetDepthCompareOp(vk::CompareOp::eLessOrEqual);
                    cmd.SetDepthTest(true, false);

                    constants.lightIndex = i;
                    cmd.PushConstants(constants);

                    auto &ids = drawOpticIDs[i];
                    scene.DrawSceneIndirect(cmd,
                        resources.GetBuffer("WarpedVertexBuffer"),
                        resources.GetBuffer(ids.drawCommandsBuffer),
                        resources.GetBuffer(ids.drawParamsBuffer));
                }
            });

        AddBufferReadback(graph,
            "OpticVisibility",
            0,
            sizeof(uint32_t) * MAX_LIGHTS * MAX_OPTICS,
            [this,
                lightCount = this->lightCount,
                optics = this->scene.opticEntities,
                lights = this->lightEntities,
                paths = this->lightPaths](BufferPtr buffer) {
                auto visibility = (const uint32_t *)buffer->Mapped();
                for (size_t lightIndex = 0; lightIndex < MAX_LIGHTS; lightIndex++) {
                    for (size_t opticIndex = 0; opticIndex < MAX_OPTICS; opticIndex++) {
                        uint32 visible = visibility[lightIndex * MAX_OPTICS + opticIndex];
                        if (visible > 1) Abortf("Uhhh");
                        if (visible > 0 && opticIndex >= optics.size()) Abortf("Optic index out of range");
                    }
                }

                previousLightPaths.clear();
                for (uint32_t lightIndex = 0; lightIndex < paths.size(); lightIndex++) {
                    auto &path = paths[lightIndex];
                    if (path.size() == 0) continue;

                    // Check if the path to the current light is still valid, else skip this light.
                    bool pathValid = true;
                    for (uint32_t i = 1; i < path.size(); i++) {
                        Assertf(path[i] < optics.size(), "Light path contains invalid index");
                        if (visibility[lightIndex * MAX_OPTICS + path[i]] == 0) {
                            pathValid = false;
                            break;
                        }
                    }
                    if (!pathValid) continue;

                    for (uint32_t opticIndex = 0; opticIndex < optics.size(); opticIndex++) {
                        if (visibility[lightIndex * MAX_OPTICS + opticIndex] == 1) {
                            Assertf(path[0] < lights.size(), "Light path contains invalid light index");
                            auto &newPath = previousLightPaths.emplace_back();
                            newPath.emplace_back(lights[path[0]]);
                            for (uint32_t i = 1; i < path.size(); i++) {
                                Assertf(path[i] < optics.size(), "Light path contains invalid optic index");
                                newPath.emplace_back(optics[path[i]]);
                            }
                            newPath.emplace_back(optics[opticIndex]);
                        }
                    }
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
