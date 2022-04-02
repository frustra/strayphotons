#include "Lighting.hh"

#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"
#include "graphics/vulkan/render_passes/Readback.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <algorithm>

namespace sp::vulkan::renderer {
    static CVar<bool> CVarVSM("r.VSM", false, "Enable Variance Shadow Mapping");
    static CVar<int> CVarPCF("r.PCF", 1, "Enable screen space shadow filtering (0: off, 1: on, 2: shadow map blur");
    static CVar<int> CVarLightingMode("r.LightingMode",
        1,
        "Toggle between different lighting shader modes "
        "(0: direct only, 1: full lighting, 2: indirect only, 3: diffuse only, 4: specular only)");

    glm::mat4 makeProjectionMatrix(glm::vec3 viewSpaceMirrorPos, glm::vec2 clip, glm::vec4 bounds) {
        return glm::mat4(
            // clang-format off
            2*clip.x,          0,                 0,                             0,
            0,                 2*clip.x,          0,                             0,
            bounds.y+bounds.x, bounds.z+bounds.w, -clip.y/(clip.y-clip.x),       -1,
            0,                  0,                -clip.y*clip.x/(clip.y-clip.x), 0
            // clang-format on
        );
    }

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
            auto viewBounds = glm::vec2(data.invProj[0][0], data.invProj[1][1]) * data.clip.x;
            data.bounds = {-viewBounds, viewBounds * 2.0f};
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

        for (auto &path : readbackLightPaths) {
            if (lightCount >= MAX_LIGHTS) break;
            if (path.size() < 2) continue;
            if (!path[0].Has<ecs::TransformSnapshot, ecs::Light>(lock)) continue;
            auto &sourceTransform = path[0].Get<ecs::TransformSnapshot>(lock);
            auto light = path[0].Get<ecs::Light>(lock);
            if (!light.on) continue;

            glm::vec3 lightOrigin = sourceTransform.GetPosition();
            glm::vec3 lightDir = sourceTransform.GetForward();
            ecs::Transform lastOpticTransform = sourceTransform;

            size_t i = 1;
            for (; i < path.size(); i++) {
                if (!path[i].Has<ecs::TransformSnapshot, ecs::OpticalElement>(lock)) break;
                auto &optic = path[i].Get<ecs::OpticalElement>(lock);
                light.tint *= optic.tint;
                if (optic.type == ecs::OpticType::Nop) {
                    auto &opticTransform = path[i].Get<ecs::TransformSnapshot>(lock);
                    lastOpticTransform = opticTransform;
                    lastOpticTransform.Rotate(M_PI, glm::vec3(0, 1, 0));
                } else if (optic.type == ecs::OpticType::Mirror) {
                    auto &opticTransform = path[i].Get<ecs::TransformSnapshot>(lock);
                    auto opticNormal = opticTransform.GetForward();
                    lastOpticTransform = opticTransform;
                    lightOrigin = glm::reflect(lightOrigin - opticTransform.GetPosition(), opticNormal) +
                                  opticTransform.GetPosition();
                    lightDir = glm::reflect(lightDir, opticNormal);
                }
            }
            if (i < path.size()) continue;

            auto &view = views[lightCount];

            ecs::Transform lightTransform = lastOpticTransform;
            lightTransform.SetPosition(lightOrigin);
            view.invViewMat = lightTransform.matrix;
            view.viewMat = glm::inverse(view.invViewMat);
            glm::vec3 lightViewMirrorPos = view.viewMat * glm::vec4(lastOpticTransform.GetPosition(), 1);

            int extent = (int)std::pow(2, light.shadowMapSize);
            view.extents = {extent, extent};
            view.fov = light.spotAngle * 2.0f;
            view.offset = {shadowAtlasSize.x, 0};
            view.clip = glm::vec2(-lightViewMirrorPos.z + 0.0001, -lightViewMirrorPos.z + 64);
            view.projMat = makeProjectionMatrix(lightViewMirrorPos,
                view.clip,
                glm::vec4(lightViewMirrorPos.x + glm::vec2(-0.5, 0.5), lightViewMirrorPos.y + glm::vec2(-0.5, 0.5)));
            view.invProjMat = glm::inverse(view.projMat);

            auto &data = gpuData.lights[lightCount];
            data.position = lightOrigin;
            data.tint = light.tint;
            data.direction = lightDir;
            data.spotAngleCos = cos(light.spotAngle);
            data.proj = view.projMat;
            data.invProj = view.invProjMat;
            data.view = view.viewMat;
            data.clip = view.clip;
            data.mapOffset = {shadowAtlasSize.x, 0, extent, extent};
            data.bounds = {lightViewMirrorPos.x - 0.5, lightViewMirrorPos.y - 0.5, 1, 1};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;

            data.gelId = 0;
            if (!light.gelName.empty()) {
                auto it = gelTextureCache.emplace(light.gelName, 0).first;
                gelTextures[lightCount] = std::make_pair(light.gelName, &it->second);
            } else {
                gelTextures[lightCount] = {};
            }

            auto &lightPath = lightPaths.emplace_back();
            auto foundIndex = std::find(lightEntities.begin(), lightEntities.end(), path[0]) - lightEntities.begin();
            std::string str = ecs::ToString(lock, path[0]) + ":" + std::to_string(foundIndex);
            lightPath.push_back(foundIndex);
            for (i = 1; i < path.size(); i++) {
                foundIndex = std::find(scene.opticEntities.begin(), scene.opticEntities.end(), path[i]) -
                             scene.opticEntities.begin();
                // str += "," + ecs::ToString(lock, path[i]) + ":" + std::to_string(foundIndex);
                lightPath.push_back(foundIndex);
            }
            // Logf("Visible path: %s", str);

            shadowAtlasSize.x += extent;
            if (extent > shadowAtlasSize.y) shadowAtlasSize.y = extent;
            if (++lightCount >= MAX_LIGHTS) break;
        }

        glm::vec4 mapOffsetScale(shadowAtlasSize, shadowAtlasSize);
        for (uint32_t i = 0; i < lightCount; i++) {
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
        graph.BeginScope("ShadowMap");
        // vector<GPUScene::DrawBufferIDs> drawAllIDs, drawOpticIDs;
        // drawAllIDs.reserve(lightCount);
        // drawOpticIDs.reserve(lightCount);
        // for (uint32_t i = 0; i < lightCount; i++) {
        //     drawAllIDs.push_back(scene.GenerateDrawsForView(graph, views[i].visibilityMask));

        //     auto opticMask = views[i].visibilityMask;
        //     opticMask.set(ecs::Renderable::VISIBLE_OPTICS);
        //     drawOpticIDs.push_back(scene.GenerateDrawsForView(graph, opticMask));
        // }
        ecs::Renderable::VisibilityMask opticMask;
        opticMask.set(ecs::Renderable::VISIBLE_LIGHTING_SHADOW);
        auto drawAllIDs = scene.GenerateDrawsForView(graph, opticMask);
        opticMask.set(ecs::Renderable::VISIBLE_OPTICS);
        auto drawOpticIDs = scene.GenerateDrawsForView(graph, opticMask);

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

        graph.AddPass("RenderDepth")
            .Build([&](rg::PassBuilder &builder) {
                ImageDesc desc;
                auto extent = glm::max(glm::ivec2(1), shadowAtlasSize);
                desc.extent = vk::Extent3D(extent.x, extent.y, 1);

                desc.format = CVarVSM.Get() ? vk::Format::eR32G32Sfloat : vk::Format::eR32Sfloat;
                builder.OutputColorAttachment(0, "Linear", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eD16Unorm;
                builder.OutputDepthAttachment("Depth", desc, {LoadOp::Clear, StoreOp::Store});

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);

                // for (auto &ids : drawAllIDs) {
                builder.Read(drawAllIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawAllIDs.drawParamsBuffer, Access::VertexShaderReadStorage);
                // }
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

                    // auto &ids = drawAllIDs[i];
                    scene.DrawSceneIndirect(cmd,
                        resources.GetBuffer("WarpedVertexBuffer"),
                        resources.GetBuffer(drawAllIDs.drawCommandsBuffer),
                        resources.GetBuffer(drawAllIDs.drawParamsBuffer));
                }
            });

        graph.AddPass("OpticsVisibility")
            .Build([&](rg::PassBuilder &builder) {
                builder.SetDepthAttachment("Depth", {LoadOp::Load, StoreOp::Store});

                builder.Write("OpticVisibility", Access::FragmentShaderWrite);
                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);

                // for (auto &ids : drawOpticIDs) {
                builder.Read(drawOpticIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawOpticIDs.drawParamsBuffer, Access::VertexShaderReadStorage);
                // }
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

                    // auto &ids = drawOpticIDs[i];
                    scene.DrawSceneIndirect(cmd,
                        resources.GetBuffer("WarpedVertexBuffer"),
                        resources.GetBuffer(drawOpticIDs.drawCommandsBuffer),
                        resources.GetBuffer(drawOpticIDs.drawParamsBuffer));
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
                auto visibility = (const std::array<uint32_t, MAX_OPTICS> *)buffer->Mapped();
                for (uint32_t lightIndex = 0; lightIndex < MAX_LIGHTS; lightIndex++) {
                    for (uint32_t opticIndex = 0; opticIndex < MAX_OPTICS; opticIndex++) {
                        uint32 visible = visibility[lightIndex][opticIndex];
                        if (visible > 1) {
                            Tracef("Uhhh");
                            Abortf("Uhhh");
                            lightIndex = MAX_LIGHTS;
                            break;
                        }
                        if (visible == 1 && opticIndex >= optics.size()) Abortf("Optic index out of range");
                    }
                }

                readbackLightPaths.clear();
                InlineVector<bool, MAX_LIGHTS> lightValid;
                lightValid.resize(paths.size());
                std::fill(lightValid.begin(), lightValid.end(), true);
                for (uint32_t lightIndex = 0; lightIndex < paths.size(); lightIndex++) {
                    auto &path = paths[lightIndex];
                    if (path.size() == 0 || path.size() >= path.capacity()) continue;

                    // Check if the path to the current light is still valid, else skip this light.
                    for (uint32_t i = 1; i < path.size(); i++) {
                        Assertf(path[i] < optics.size(), "Light path contains invalid index");
                        // TODO: Fix his broken logic, the index isn't correct
                        if (visibility[lightIndex][path[i]] == 0) {
                            // lightValid[lightIndex] = false;
                            break;
                        }
                    }
                    if (!lightValid[lightIndex]) continue;

                    for (uint32_t opticIndex = 0; opticIndex < optics.size(); opticIndex++) {
                        if (visibility[lightIndex][opticIndex] == 1) {
                            Assertf(path[0] < lights.size(), "Light path contains invalid light index");
                            auto &newPath = readbackLightPaths.emplace_back();
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
        graph.EndScope();

        graph.BeginScope("ShadowMapBlur");
        auto sourceID = graph.LastOutputID();
        auto blurY1 = AddGaussianBlur1D(graph, sourceID, glm::ivec2(0, 1), 1);
        AddGaussianBlur1D(graph, blurY1, glm::ivec2(1, 0), 2);
        graph.EndScope();
    }

    void Lighting::AddLightingPass(RenderGraph &graph) {
        auto depthTarget = (CVarVSM.Get() || CVarPCF.Get() == 2) ? "ShadowMapBlur.LastOutput" : "ShadowMap.Linear";

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
