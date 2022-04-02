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
        shadowAtlasSize = glm::ivec2(0, 0);
        gelTextureCache.clear();
        lights.clear();

        for (auto entity : lock.EntitiesWith<ecs::Light>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &light = entity.Get<ecs::Light>(lock);
            if (!light.on) continue;

            auto &vLight = lights.emplace_back();
            vLight.source = entity;
            vLight.sourceIndex = lights.size() - 1;
            vLight.opticIndex = ~(uint32_t)0;

            int extent = (int)std::pow(2, light.shadowMapSize);

            auto &transform = entity.Get<ecs::TransformSnapshot>(lock);

            vLight.view.extents = {extent, extent};
            vLight.view.fov = light.spotAngle * 2.0f;
            vLight.view.offset = {shadowAtlasSize.x, 0};
            vLight.view.clip = light.shadowMapClip;
            vLight.view.UpdateProjectionMatrix();
            vLight.view.UpdateViewMatrix(lock, entity);

            auto &data = gpuData.lights[lights.size() - 1];
            data.position = transform.GetPosition();
            data.tint = light.tint;
            data.direction = transform.GetForward();
            data.spotAngleCos = cos(light.spotAngle);
            data.proj = vLight.view.projMat;
            data.invProj = vLight.view.invProjMat;
            data.view = vLight.view.viewMat;
            data.clip = vLight.view.clip;
            data.mapOffset = {shadowAtlasSize.x, 0, extent, extent};
            auto viewBounds = glm::vec2(data.invProj[0][0], data.invProj[1][1]) * data.clip.x;
            data.bounds = {-viewBounds, viewBounds * 2.0f};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;

            data.gelId = 0;
            if (!light.gelName.empty()) {
                auto it = gelTextureCache.emplace(light.gelName, 0).first;
                vLight.gelName = light.gelName;
                vLight.gelTexture = &it->second;
            }

            shadowAtlasSize.x += extent;
            if (extent > shadowAtlasSize.y) shadowAtlasSize.y = extent;
            if (lights.size() >= MAX_LIGHTS) break;
        }

        for (uint32_t lightIndex = 0; lightIndex < readbackLights.size(); lightIndex++) {
            if (lights.size() >= MAX_LIGHTS) break;
            if (readbackLights[lightIndex].sourceIndex == lightIndex) continue;

            InlineVector<ecs::Entity, MAX_LIGHTS> path;
            uint32_t pathIndex = lightIndex;
            while (pathIndex < readbackLights.size() && readbackLights[pathIndex].sourceIndex != pathIndex) {
                path.emplace_back(readbackLights[pathIndex].source);
                pathIndex = readbackLights[pathIndex].sourceIndex;
            }
            if (pathIndex >= readbackLights.size()) break;
            path.emplace_back(readbackLights[pathIndex].source);

            auto &sourceLight = path.back();
            if (!sourceLight.Has<ecs::TransformSnapshot, ecs::Light>(lock)) continue;
            auto &sourceTransform = sourceLight.Get<ecs::TransformSnapshot>(lock);
            auto light = sourceLight.Get<ecs::Light>(lock);
            if (!light.on) continue;

            glm::vec3 lightOrigin = sourceTransform.GetPosition();
            glm::vec3 lightDir = sourceTransform.GetForward();
            ecs::Transform lastOpticTransform = sourceTransform;

            int i = path.size() - 2;
            for (; i >= 0; i--) {
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
            if (i >= 0) continue;

            auto &vLight = lights.emplace_back();
            vLight.source = path.front();
            vLight.sourceIndex = std::find_if(lights.begin(), lights.end(), [search = path[1]](auto &&light) {
                // TODO: Fix this, it can find the wrong entity if the path hits the same mirror twice
                return light.source == search;
            }) - lights.begin();
            vLight.opticIndex = std::find(scene.opticEntities.begin(), scene.opticEntities.end(), vLight.source) -
                                scene.opticEntities.begin();

            ecs::Transform lightTransform = lastOpticTransform;
            lightTransform.SetPosition(lightOrigin);
            vLight.view.invViewMat = lightTransform.matrix;
            vLight.view.viewMat = glm::inverse(vLight.view.invViewMat);
            glm::vec3 lightViewMirrorPos = vLight.view.viewMat * glm::vec4(lastOpticTransform.GetPosition(), 1);

            int extent = (int)std::pow(2, light.shadowMapSize);
            vLight.view.extents = {extent, extent};
            vLight.view.fov = light.spotAngle * 2.0f;
            vLight.view.offset = {shadowAtlasSize.x, 0};
            vLight.view.clip = glm::vec2(-lightViewMirrorPos.z + 0.0001, -lightViewMirrorPos.z + 64);
            vLight.view.projMat = makeProjectionMatrix(lightViewMirrorPos,
                vLight.view.clip,
                glm::vec4(lightViewMirrorPos.x + glm::vec2(-0.5, 0.5), lightViewMirrorPos.y + glm::vec2(-0.5, 0.5)));
            vLight.view.invProjMat = glm::inverse(vLight.view.projMat);

            auto &data = gpuData.lights[lights.size() - 1];
            data.position = lightOrigin;
            data.tint = light.tint;
            data.direction = lightDir;
            data.spotAngleCos = cos(light.spotAngle);
            data.proj = vLight.view.projMat;
            data.invProj = vLight.view.invProjMat;
            data.view = vLight.view.viewMat;
            data.clip = vLight.view.clip;
            data.mapOffset = {shadowAtlasSize.x, 0, extent, extent};
            data.bounds = {lightViewMirrorPos.x - 0.5, lightViewMirrorPos.y - 0.5, 1, 1};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;

            data.gelId = 0;
            if (!light.gelName.empty()) {
                auto it = gelTextureCache.emplace(light.gelName, 0).first;
                vLight.gelName = light.gelName;
                vLight.gelTexture = &it->second;
            }

            shadowAtlasSize.x += extent;
            if (extent > shadowAtlasSize.y) shadowAtlasSize.y = extent;
        }

        glm::vec4 mapOffsetScale(shadowAtlasSize, shadowAtlasSize);
        for (uint32_t i = 0; i < lights.size(); i++) {
            gpuData.lights[i].mapOffset /= mapOffsetScale;
        }
        gpuData.count = lights.size();

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
                for (size_t i = 0; i < lights.size() && i < MAX_LIGHTS; i++) {
                    if (lights[i].gelTexture) gpuData.lights[i].gelId = *lights[i].gelTexture;
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

                for (uint32_t i = 0; i < lights.size(); i++) {
                    auto &view = lights[i].view;

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

                for (uint32_t i = 0; i < lights.size(); i++) {
                    auto &view = lights[i].view;

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
            [this, lights = this->lights, optics = this->scene.opticEntities](BufferPtr buffer) {
                auto visibility = (const std::array<uint32_t, MAX_OPTICS> *)buffer->Mapped();
                for (uint32_t lightIndex = 0; lightIndex < MAX_LIGHTS; lightIndex++) {
                    for (uint32_t opticIndex = 0; opticIndex < MAX_OPTICS; opticIndex++) {
                        uint32 visible = visibility[lightIndex][opticIndex];
                        if (visible > 1) {
                            Tracef("Uhhh");
                            Abortf("Uhhh");
                        }
                        if (visible == 1 && opticIndex >= optics.size()) Abortf("Optic index out of range");
                    }
                }

                readbackLights.clear();
                InlineVector<bool, MAX_LIGHTS> lightValid;
                lightValid.resize(lights.size());
                std::fill(lightValid.begin(), lightValid.end(), true);
                for (uint32_t lightIndex = 0; lightIndex < lights.size(); lightIndex++) {
                    // Forward all real lights first
                    if (lights[lightIndex].sourceIndex != lightIndex || readbackLights.size() >= MAX_LIGHTS) break;
                    readbackLights.emplace_back(lights[lightIndex]);
                }
                for (uint32_t lightIndex = 0; lightIndex < lights.size(); lightIndex++) {
                    auto &vLight = lights[lightIndex];
                    if (vLight.sourceIndex >= lights.size()) continue;

                    // Check if the path to the current light is still valid, else skip this light.
                    if (vLight.sourceIndex != lightIndex) {
                        if (!lightValid[vLight.sourceIndex]) continue;
                        Assertf(vLight.opticIndex < MAX_OPTICS,
                            "Virtual light optic index is out of range: %u",
                            vLight.opticIndex);
                        if (visibility[vLight.sourceIndex][vLight.opticIndex] != 1) {
                            lightValid[lightIndex] = false;
                            continue;
                        }
                    }

                    // Check if any optics are visible from the end of the current light path.
                    for (uint32_t opticIndex = 0; opticIndex < optics.size(); opticIndex++) {
                        if (opticIndex == vLight.opticIndex) continue;
                        if (visibility[lightIndex][opticIndex] == 1) {
                            auto &newLight = readbackLights.emplace_back(vLight);
                            newLight.source = optics[opticIndex];
                            newLight.opticIndex = opticIndex;
                            newLight.sourceIndex = lightIndex;
                            if (readbackLights.size() >= MAX_LIGHTS) break;
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
