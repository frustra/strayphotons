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

    static glm::mat4 makeOpticProjectionMatrix(glm::vec2 clip, glm::vec4 bounds) {
        return glm::mat4(
            // clang-format off
            2*clip.x,          0,                 0,                              0,
            0,                 2*clip.x,          0,                              0,
            bounds.y+bounds.x, bounds.z+bounds.w, -clip.y/(clip.y-clip.x),       -1,
            0,                 0,                 -clip.y*clip.x/(clip.y-clip.x), 0
            // clang-format on
        );
    }

    void Lighting::LoadState(RenderGraph &graph,
        ecs::Lock<ecs::Read<ecs::Light, ecs::OpticalElement, ecs::TransformSnapshot>> lock) {
        ZoneScoped;
        gelTextureCache.clear();
        lights.clear();

        for (auto entity : lock.EntitiesWith<ecs::Light>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &light = entity.Get<ecs::Light>(lock);
            auto &gelName = light.gelName;

            if (!gelName.empty() && gelTextureCache.find(gelName) == gelTextureCache.end()) {
                if (starts_with(gelName, "graph:")) {
                    gelTextureCache[gelName] = {};
                } else if (starts_with(gelName, "asset:")) {
                    auto handle = scene.textures.LoadAssetImage(gelName.substr(6));
                    gelTextureCache[gelName] = handle;

                    // cull lights that don't have their gel loaded yet
                    if (!handle.Ready()) continue;
                }
            }

            if (!light.on) continue;

            auto &vLight = lights.emplace_back();
            vLight.lightPath = {entity};

            int extent = (int)std::pow(2, light.shadowMapSize);

            auto &transform = entity.Get<ecs::TransformSnapshot>(lock);

            auto &view = views[lights.size() - 1];
            view.extents = {extent, extent};
            view.fov = light.spotAngle * 2.0f;
            view.clip = light.shadowMapClip;
            view.UpdateProjectionMatrix();
            view.UpdateViewMatrix(lock, entity);

            auto &data = gpuData.lights[lights.size() - 1];
            data.position = transform.GetPosition();
            data.tint = light.tint;
            data.direction = transform.GetForward();
            data.spotAngleCos = cos(light.spotAngle);
            data.proj = view.projMat;
            data.invProj = view.invProjMat;
            data.view = view.viewMat;
            data.clip = view.clip;
            auto viewBounds = glm::vec2(data.invProj[0][0], data.invProj[1][1]) * data.clip.x;
            data.bounds = {-viewBounds, viewBounds * 2.0f};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;
            data.gelId = 0;

            if (!gelName.empty()) {
                vLight.gelName = gelName;
                vLight.gelTexture = &gelTextureCache[gelName].index;
            }

            if (lights.size() >= MAX_LIGHTS) break;
        }

        for (uint32_t lightIndex = 0; lightIndex < readbackLights.size(); lightIndex++) {
            if (lights.size() >= MAX_LIGHTS) break;
            auto &rbLight = readbackLights[lightIndex];

            auto &sourceLight = rbLight.lightPath.front();
            if (!sourceLight.Has<ecs::TransformSnapshot, ecs::Light>(lock)) continue;
            auto &sourceTransform = sourceLight.Get<ecs::TransformSnapshot>(lock);
            auto light = sourceLight.Get<ecs::Light>(lock);
            if (!light.on) continue;

            glm::vec3 lightOrigin = sourceTransform.GetPosition();
            glm::vec3 lightDir = sourceTransform.GetForward();
            ecs::Transform lastOpticTransform = sourceTransform;

            size_t i = 1;
            for (; i < rbLight.lightPath.size(); i++) {
                if (!rbLight.lightPath[i].Has<ecs::TransformSnapshot, ecs::OpticalElement>(lock)) break;
                auto &optic = rbLight.lightPath[i].Get<ecs::OpticalElement>(lock);
                light.tint *= optic.tint;
                if (light.tint == glm::vec3(0)) break;
                if (optic.type == ecs::OpticType::Gel) {
                    auto &opticTransform = rbLight.lightPath[i].Get<ecs::TransformSnapshot>(lock);
                    lastOpticTransform = opticTransform;
                    lastOpticTransform.Rotate(M_PI, glm::vec3(0, 1, 0));
                } else if (optic.type == ecs::OpticType::Mirror) {
                    auto &opticTransform = rbLight.lightPath[i].Get<ecs::TransformSnapshot>(lock);
                    auto opticNormal = opticTransform.GetForward();
                    lastOpticTransform = opticTransform;
                    lightOrigin = glm::reflect(lightOrigin - opticTransform.GetPosition(), opticNormal) +
                                  opticTransform.GetPosition();
                    lightDir = glm::reflect(lightDir, opticNormal);
                }
            }
            if (i < rbLight.lightPath.size()) continue;

            auto &vLight = lights.emplace_back(rbLight);
            vLight.parentIndex = std::find_if(lights.begin(), lights.end(), [&vLight](auto &light) {
                return light.lightPath.size() + 1 == vLight.lightPath.size() &&
                       std::equal(light.lightPath.begin(), light.lightPath.end(), vLight.lightPath.begin());
            }) - lights.begin();
            vLight.opticIndex = std::find(scene.opticEntities.begin(),
                                    scene.opticEntities.end(),
                                    vLight.lightPath.back()) -
                                scene.opticEntities.begin();

            auto &view = views[lights.size() - 1];
            ecs::Transform lightTransform = lastOpticTransform;
            lightTransform.SetPosition(lightOrigin);
            view.invViewMat = lightTransform.matrix;
            view.viewMat = glm::inverse(view.invViewMat);
            glm::vec3 lightViewMirrorPos = view.viewMat * glm::vec4(lastOpticTransform.GetPosition(), 1);

            int extent = (int)std::pow(2, light.shadowMapSize);
            view.extents = {extent, extent};
            view.fov = light.spotAngle * 2.0f;
            view.clip = glm::vec2(-lightViewMirrorPos.z + 0.0001, -lightViewMirrorPos.z + 64);
            view.projMat = makeOpticProjectionMatrix(view.clip,
                glm::vec4(lightViewMirrorPos.x + glm::vec2(-0.5, 0.5), lightViewMirrorPos.y + glm::vec2(-0.5, 0.5)));
            view.invProjMat = glm::inverse(view.projMat);

            auto &data = gpuData.lights[lights.size() - 1];
            data.position = lightOrigin;
            data.tint = light.tint;
            data.direction = lightDir;
            data.spotAngleCos = cos(light.spotAngle);
            data.proj = view.projMat;
            data.invProj = view.invProjMat;
            data.view = view.viewMat;
            data.clip = view.clip;
            data.bounds = {lightViewMirrorPos.x - 0.5, lightViewMirrorPos.y - 0.5, 1, 1};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;

            data.gelId = 0;
            if (!light.gelName.empty()) {
                vLight.gelName = light.gelName;
                vLight.gelTexture = &gelTextureCache[light.gelName].index;
            }
        }

        gpuData.count = lights.size();
        AllocateShadowMap();

        graph.AddPass("LightState")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateUniform("LightState", sizeof(gpuData));
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                resources.GetBuffer("LightState")->CopyFrom(&gpuData);
            });
    }

    void Lighting::AllocateShadowMap() {
        uint64 totalPixels = 0;
        for (uint32 i = 0; i < lights.size(); i++) {
            auto extents = views[i].extents;
            totalPixels += extents.x * extents.y;
        }

        uint32 width = CeilToPowerOfTwo((uint32)sqrt((double)totalPixels));
        shadowAtlasSize = glm::ivec2(width, width);

        freeRectangles.clear();
        freeRectangles.push_back({{0, 0}, {width, width}});

        glm::vec4 mapOffsetScale(shadowAtlasSize, shadowAtlasSize);
        for (uint32 i = 0; i < lights.size(); i++) {
            auto extents = views[i].extents;
            int rectIndex = -1;
            for (int r = freeRectangles.size() - 1; r >= 0; r--) {
                if (glm::all(glm::greaterThanEqual(freeRectangles[r].second, extents))) {
                    if (rectIndex == -1 ||
                        glm::all(glm::lessThan(freeRectangles[r].second, freeRectangles[rectIndex].second))) {
                        rectIndex = r;
                    }
                }
            }
            Assert(rectIndex >= 0, "ran out of shadow map space");

            while (glm::all(glm::greaterThan(freeRectangles[rectIndex].second, extents))) {
                auto rect = freeRectangles[rectIndex];
                auto freeExtent = rect.second - rect.second / 2;
                rect.second /= 2;
                freeRectangles[rectIndex].second = rect.second;

                freeRectangles.push_back({{rect.first.x, rect.first.y + rect.second.y}, {rect.second.x, freeExtent.y}});
                freeRectangles.push_back({{rect.first.x + rect.second.x, rect.first.y}, {freeExtent.x, rect.second.y}});
                freeRectangles.push_back({{rect.first.x + rect.second.x, rect.first.y + rect.second.y}, freeExtent});
            }

            auto rect = freeRectangles[rectIndex];
            views[i].offset = rect.first;
            gpuData.lights[i].mapOffset = glm::vec4{rect.first.x, rect.first.y, rect.second.x, rect.second.y} /
                                          mapOffsetScale;

            freeRectangles.erase(freeRectangles.begin() + rectIndex);
        }
    }

    void Lighting::AddGelTextures(RenderGraph &graph) {
        graph.AddPass("GelTextures")
            .Build([&](rg::PassBuilder &builder) {
                for (auto &gel : gelTextureCache) {
                    if (gel.second.index != 0) continue;
                    builder.Read(gel.first.substr(6), Access::FragmentShaderSampleImage);
                }
                builder.Write("LightState", Access::HostWrite);
            })
            .Execute([this](rg::Resources &resources, DeviceContext &device) {
                for (auto &gel : gelTextureCache) {
                    if (gel.second.index != 0) continue;
                    gel.second = scene.textures.Add(resources.GetImageView(gel.first.substr(6)));
                }
                for (size_t i = 0; i < lights.size() && i < MAX_LIGHTS; i++) {
                    if (lights[i].gelTexture) gpuData.lights[i].gelId = *lights[i].gelTexture;
                }
                resources.GetBuffer("LightState")->CopyFrom(&gpuData);
            });
    }

    void Lighting::AddShadowPasses(RenderGraph &graph) {
        ZoneScoped;
        graph.BeginScope("ShadowMap");

        auto drawAllIDs = scene.GenerateDrawsForView(graph, ecs::VisibilityMask::LightingShadow);
        auto drawOpticIDs = scene.GenerateDrawsForView(graph, ecs::VisibilityMask::Optics);

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

                builder.Read(drawAllIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawAllIDs.drawParamsBuffer, Access::VertexShaderReadStorage);
            })
            .Execute([this, drawAllIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("shadow_map.vert", CVarVSM.Get() ? "shadow_map_vsm.frag" : "shadow_map.frag");

                for (uint32_t i = 0; i < lights.size(); i++) {
                    GPUViewState lightViews[] = {{views[i]}, {}};
                    cmd.UploadUniformData(0, 0, lightViews, 2);

                    vk::Rect2D viewport;
                    viewport.extent = vk::Extent2D(views[i].extents.x, views[i].extents.y);
                    viewport.offset = vk::Offset2D(views[i].offset.x, views[i].offset.y);
                    cmd.SetViewport(viewport);
                    cmd.SetYDirection(YDirection::Down);

                    scene.DrawSceneIndirect(cmd,
                        resources.GetBuffer("WarpedVertexBuffer"),
                        resources.GetBuffer(drawAllIDs.drawCommandsBuffer),
                        resources.GetBuffer(drawAllIDs.drawParamsBuffer));
                }
            });

        graph.AddPass("OpticsVisibility")
            .Build([&](rg::PassBuilder &builder) {
                builder.SetDepthAttachment("Depth", {LoadOp::Load, StoreOp::ReadOnly});

                builder.Write("OpticVisibility", Access::FragmentShaderWrite);
                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);

                builder.Read(drawOpticIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawOpticIDs.drawParamsBuffer, Access::VertexShaderReadStorage);
            })
            .Execute([this, drawOpticIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("optic_visibility.vert", "optic_visibility.frag");

                struct {
                    uint32_t lightIndex;
                } constants;

                auto visBuffer = resources.GetBuffer("OpticVisibility");

                for (uint32_t i = 0; i < lights.size(); i++) {
                    GPUViewState lightViews[] = {{views[i]}, {}};
                    cmd.UploadUniformData(0, 0, lightViews, 2);
                    cmd.SetStorageBuffer(0, 1, visBuffer);

                    vk::Rect2D viewport;
                    viewport.extent = vk::Extent2D(views[i].extents.x, views[i].extents.y);
                    viewport.offset = vk::Offset2D(views[i].offset.x, views[i].offset.y);
                    cmd.SetViewport(viewport);
                    cmd.SetYDirection(YDirection::Down);
                    cmd.SetDepthCompareOp(vk::CompareOp::eLessOrEqual);
                    cmd.SetDepthTest(true, false);

                    constants.lightIndex = i;
                    cmd.PushConstants(constants);

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
                ZoneScopedN("OpticVisibilityReadback");
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
                    auto &vLight = lights[lightIndex];
                    if (vLight.lightPath.size() >= MAX_LIGHTS) continue;

                    // Check if the path to the current light is still valid, else skip this light.
                    if (vLight.parentIndex && vLight.opticIndex) {
                        Assertf(*vLight.parentIndex < lightValid.size(), "Virtual light parent index is out of range");
                        if (!lightValid[*vLight.parentIndex]) {
                            lightValid[lightIndex] = false;
                            continue;
                        }
                        Assertf(vLight.opticIndex < MAX_OPTICS, "Virtual light optic index is out of range");
                        if (visibility[*vLight.parentIndex][*vLight.opticIndex] != 1) {
                            lightValid[lightIndex] = false;
                            continue;
                        }
                    }

                    // Check if any optics are visible from the end of the current light path.
                    for (uint32_t opticIndex = 0; opticIndex < optics.size(); opticIndex++) {
                        if (readbackLights.size() >= MAX_LIGHTS) break;
                        if (vLight.opticIndex && opticIndex == *vLight.opticIndex) continue;
                        if (visibility[lightIndex][opticIndex] == 1) {
                            auto &newLight = readbackLights.emplace_back(vLight);
                            newLight.parentIndex = lightIndex;
                            newLight.opticIndex = opticIndex;
                            newLight.lightPath.emplace_back(optics[opticIndex]);
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
        auto shadowDepth = (CVarVSM.Get() || CVarPCF.Get() == 2) ? "ShadowMapBlur.LastOutput" : "ShadowMap.Linear";

        graph.AddPass("Lighting")
            .Build([&](rg::PassBuilder &builder) {
                auto gBuffer0 = builder.Read("GBuffer0", Access::FragmentShaderSampleImage);
                builder.Read("GBuffer1", Access::FragmentShaderSampleImage);
                builder.Read("GBuffer2", Access::FragmentShaderSampleImage);
                builder.Read(shadowDepth, Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Radiance", Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Normals", Access::FragmentShaderSampleImage);

                auto desc = builder.DeriveImage(gBuffer0);
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(0, "LinearLuminance", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ReadUniform("VoxelState");
                builder.Read("ExposureState", Access::FragmentShaderReadStorage);
                builder.ReadUniform("ViewState");
                builder.ReadUniform("LightState");

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::ReadOnly});
            })
            .Execute([this, shadowDepth](rg::Resources &resources, CommandContext &cmd) {
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
                cmd.SetImageView(0, 3, resources.GetImageDepthView("GBufferDepthStencil"));
                cmd.SetImageView(0, 4, resources.GetImageView(shadowDepth));
                cmd.SetImageView(0, 5, resources.GetImageView("Voxels.Radiance"));
                cmd.SetImageView(0, 6, resources.GetImageView("Voxels.Normals"));

                cmd.SetUniformBuffer(0, 8, resources.GetBuffer("VoxelState"));
                cmd.SetStorageBuffer(0, 9, resources.GetBuffer("ExposureState"));
                cmd.SetUniformBuffer(0, 10, resources.GetBuffer("ViewState"));
                cmd.SetUniformBuffer(0, 11, resources.GetBuffer("LightState"));

                cmd.SetBindlessDescriptors(1, scene.textures.GetDescriptorSet());

                cmd.SetShaderConstant(ShaderStage::Fragment, 0, CVarLightingMode.Get());
                cmd.Draw(3);
            });
    }
} // namespace sp::vulkan::renderer
