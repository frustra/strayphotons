/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Lighting.hh"

#include "ecs/EcsImpl.hh"
#include "game/Scene.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_passes/Blur.hh"
#include "graphics/vulkan/render_passes/Readback.hh"
#include "graphics/vulkan/render_passes/Voxels.hh"
#include "graphics/vulkan/scene/GPUScene.hh"

#include <algorithm>
#include <glm/gtx/vector_angle.hpp>

namespace sp::vulkan::renderer {
    CVar<int> CVarShadowMapSampleCount("r.ShadowMapSampleCount", 9, "Number of samples to use in shadow filtering");
    CVar<float> CVarShadowMapSampleWidth("r.ShadowMapSampleWidth",
        4.0,
        "The size of filter used for filtering shadows");

    static CVar<bool> CVarBlurShadowMap("r.BlurShadowMap", false, "Blur the shadow map before sampling");
    static CVar<int> CVarShadowMapSizeOffset("r.ShadowMapSizeOffset", -1, "Adjust shadow map sizes by 2^N");
    static CVar<bool> CVarPCF("r.PCF", true, "Enable screen space shadow filtering (0: off, 1: on)");
    static CVar<int> CVarLightingMode("r.LightingMode",
        1,
        "Toggle between different lighting shader modes "
        "(0: direct only, 1: full lighting, 2: indirect only, 3: diffuse only, 4: specular only)");
    static CVar<uint32_t> CVarLightingVoxelLayers("r.LightingVoxelLayers",
        8,
        "Number of voxel layers to use for diffuse lighting");

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

    static glm::vec3 viewPosToScreenPos(glm::vec4 viewPos, glm::mat4 projMat) {
        auto clip = projMat * viewPos;
        return glm::vec3(clip) / clip.w * glm::vec3(0.5, 0.5, 1) + glm::vec3(0.5, 0.5, 0);
    }

    glm::vec2 bilinearMix(glm::vec2 v00, glm::vec2 v10, glm::vec2 v01, glm::vec2 v11, glm::vec2 alpha) {
        glm::vec2 x1 = glm::mix(v00, v10, alpha.x);
        glm::vec2 x2 = glm::mix(v01, v11, alpha.x);
        return glm::mix(x1, x2, alpha.y);
    }

    void Lighting::LoadState(RenderGraph &graph,
        ecs::Lock<ecs::Read<ecs::Light, ecs::OpticalElement, ecs::TransformSnapshot>> lock) {
        ZoneScoped;
        lights.clear();

        for (auto entity : lock.EntitiesWith<ecs::Light>()) {
            if (!entity.Has<ecs::TransformSnapshot>(lock)) continue;

            auto &light = entity.Get<ecs::Light>(lock);
            auto &filterName = light.filterName;

            if (!filterName.empty() && scene.textureCache.find(filterName) == scene.textureCache.end()) {
                // cull lights that don't have their filter texture loaded yet
                continue;
            }

            if (!light.on) continue;

            auto &vLight = lights.emplace_back();
            vLight.source = entity;

            int mapSize = std::clamp((int)light.shadowMapSize + CVarShadowMapSizeOffset.Get(), 0, 16);
            int extent = 1 << mapSize; // 2^N map size

            auto &transform = entity.Get<ecs::TransformSnapshot>(lock).globalPose;

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
            data.invView = view.invViewMat;
            data.clip = view.clip;
            auto viewBounds = glm::vec2(data.invProj[0][0], data.invProj[1][1]) * data.clip.x;
            data.bounds = {-viewBounds, viewBounds * 2.0f};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;

            data.filterId = 0;
            if (!filterName.empty()) {
                data.cornerUVs = {
                    glm::vec2(0, 0),
                    glm::vec2(0, 1),
                    glm::vec2(1, 1),
                    glm::vec2(1, 0),
                };
                vLight.filterName = filterName;
                vLight.filterTexture = scene.textureCache[filterName].index;
            }

            data.previousIndex = std::find(previousLights.begin(), previousLights.end(), vLight) -
                                 previousLights.begin();
            data.parentIndex = ~0u;

            if (lights.size() >= MAX_LIGHTS) break;
        }

        for (uint32_t lightIndex = 0; lightIndex < readbackLights.size(); lightIndex++) {
            if (lights.size() >= MAX_LIGHTS) break;
            auto &rbLight = readbackLights[lightIndex];

            if (!rbLight.source.Has<ecs::TransformSnapshot, ecs::Light>(lock)) continue;
            auto &sourceTransform = rbLight.source.Get<ecs::TransformSnapshot>(lock).globalPose;
            auto light = rbLight.source.Get<ecs::Light>(lock);
            if (!light.on) continue;

            glm::vec3 lightOrigin = sourceTransform.GetPosition();
            glm::vec3 lightDir = sourceTransform.GetForward();
            ecs::Transform lastOpticTransform = sourceTransform;

            size_t i = 0;
            for (; i < rbLight.lightPath.size(); i++) {
                if (!rbLight.lightPath[i].ent.Has<ecs::TransformSnapshot, ecs::OpticalElement>(lock)) break;
                auto &optic = rbLight.lightPath[i].ent.Get<ecs::OpticalElement>(lock);
                bool reflect = rbLight.lightPath[i].reflect;
                if (reflect) {
                    light.tint *= optic.reflectTint;
                } else {
                    light.tint *= optic.passTint;
                }
                if (light.tint == glm::vec3(0)) break;

                lastOpticTransform = rbLight.lightPath[i].ent.Get<ecs::TransformSnapshot>(lock);
                auto opticNormal = lastOpticTransform.GetForward();
                if (reflect) {
                    lightOrigin = glm::reflect(lightOrigin - lastOpticTransform.GetPosition(), opticNormal) +
                                  lastOpticTransform.GetPosition();
                    lightDir = glm::reflect(lightDir, opticNormal);
                }

                // Make sure the optic is facing the light origin
                if (glm::dot(lastOpticTransform.GetPosition() - lightOrigin, opticNormal) < 0) {
                    lastOpticTransform.Rotate(M_PI, glm::vec3(0, 1, 0));
                }
            }
            if (i < rbLight.lightPath.size()) continue;

            auto &vLight = lights.emplace_back(rbLight);
            vLight.parentIndex = std::find_if(lights.begin(), lights.end(), [&vLight](auto &light) {
                return light.source == vLight.source && light.lightPath.size() + 1 == vLight.lightPath.size() &&
                       std::equal(light.lightPath.begin(), light.lightPath.end(), vLight.lightPath.begin());
            }) - lights.begin();
            vLight.opticIndex = std::find(scene.opticEntities.begin(),
                                    scene.opticEntities.end(),
                                    vLight.lightPath.back()) -
                                scene.opticEntities.begin();

            auto &view = views[lights.size() - 1];
            ecs::Transform lightTransform = lastOpticTransform;
            lightTransform.SetPosition(lightOrigin);
            view.invViewMat = lightTransform.GetMatrix();
            view.viewMat = glm::inverse(view.invViewMat);
            glm::vec3 lightViewMirrorPos = view.viewMat * glm::vec4(lastOpticTransform.GetPosition(), 1);

            auto cornerA = glm::normalize(lightViewMirrorPos - glm::vec3(0.5f, 0.5f, 0));
            auto cornerB = glm::normalize(lightViewMirrorPos + glm::vec3(0.5f, 0.5f, 0));
            auto calcFovDiagonal = glm::angle(cornerA, cornerB);
            float fovMultiplier = calcFovDiagonal / (light.spotAngle * 2.0f * 1.41f);
            // Clamp the multiplier, so we never go larger than 2x the original resolution
            fovMultiplier = std::clamp(fovMultiplier, 0.0f, 2.0f);

            int extent = std::pow(2, light.shadowMapSize) * fovMultiplier;
            if (extent < 32) extent = 32; // Shadowmaps below this point are useless
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
            data.invView = view.invViewMat;
            data.clip = view.clip;
            data.bounds = {lightViewMirrorPos.x - 0.5, lightViewMirrorPos.y - 0.5, 1, 1};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;

            data.filterId = 0;
            if (!light.filterName.empty()) {
                // project 4 corners of optic into filter texture uv space
                static const std::array<glm::vec2, 4> opticCornerOffsets = {
                    glm::vec2(-0.5, -0.5),
                    glm::vec2(-0.5, 0.5),
                    glm::vec2(0.5, 0.5),
                    glm::vec2(0.5, -0.5),
                };

                auto &parentLight = gpuData.lights[vLight.parentIndex.value()];

                for (int cornerI = 0; cornerI < 4; cornerI++) {
                    glm::vec4 cornerOffset(opticCornerOffsets[cornerI], 0, 1);
                    glm::vec4 cornerPointWorld(lastOpticTransform * cornerOffset, 1);
                    glm::vec4 cornerPointView = parentLight.view * cornerPointWorld;
                    glm::vec2 coord = viewPosToScreenPos(cornerPointView, parentLight.proj);

                    data.cornerUVs[cornerI] = bilinearMix(parentLight.cornerUVs[0],
                        parentLight.cornerUVs[3],
                        parentLight.cornerUVs[1],
                        parentLight.cornerUVs[2],
                        coord);
                }

                vLight.filterName = light.filterName;
                vLight.filterTexture = scene.textureCache[light.filterName].index;
            }

            data.previousIndex = std::find(previousLights.begin(), previousLights.end(), vLight) -
                                 previousLights.begin();

            data.parentIndex = std::find_if(previousLights.begin(), previousLights.end(), [&vLight](auto &light) {
                return light.source == vLight.source && light.lightPath.size() + 1 == vLight.lightPath.size() &&
                       std::equal(light.lightPath.begin(), light.lightPath.end(), vLight.lightPath.begin());
            }) - previousLights.begin();
        }
        gpuData.count = lights.size();
        previousLights = lights;

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
            auto allocExtents = std::max(CeilToPowerOfTwo((uint32)extents.x), CeilToPowerOfTwo((uint32)extents.y));
            totalPixels += allocExtents * allocExtents;
        }

        uint32 width = CeilToPowerOfTwo((uint32)ceil(sqrt((double)totalPixels)));
        shadowAtlasSize = glm::ivec2(width, width);

        freeRectangles.clear();
        freeRectangles.push_back({{0, 0}, {width, width}});

        glm::vec4 mapOffsetScale(shadowAtlasSize, shadowAtlasSize);
        for (uint32 i = 0; i < lights.size(); i++) {
            auto extents = views[i].extents;
            auto allocExtents = glm::ivec2(CeilToPowerOfTwo((uint32)extents.x), CeilToPowerOfTwo((uint32)extents.y));
            int rectIndex = -1;
            for (int r = freeRectangles.size() - 1; r >= 0; r--) {
                if (glm::all(glm::greaterThanEqual(freeRectangles[r].second, extents))) {
                    if (rectIndex == -1 ||
                        glm::all(glm::lessThanEqual(freeRectangles[r].second, freeRectangles[rectIndex].second))) {
                        rectIndex = r;
                    }
                }
            }
            Assert(rectIndex >= 0, "ran out of shadow map space");

            while (glm::all(glm::greaterThan(freeRectangles[rectIndex].second, allocExtents))) {
                auto rect = freeRectangles[rectIndex];
                rect.second /= 2;
                freeRectangles[rectIndex].second = rect.second;

                freeRectangles.push_back({{rect.first.x + rect.second.x, rect.first.y}, rect.second});
                freeRectangles.push_back({{rect.first.x, rect.first.y + rect.second.y}, rect.second});
                freeRectangles.push_back({{rect.first.x + rect.second.x, rect.first.y + rect.second.y}, rect.second});
            }

            auto rect = freeRectangles[rectIndex];
            views[i].offset = rect.first;
            views[i].extents = rect.second;
            gpuData.lights[i].mapOffset = glm::vec4{rect.first.x, rect.first.y, rect.second.x, rect.second.y} /
                                          mapOffsetScale;

            freeRectangles.erase(freeRectangles.begin() + rectIndex);
        }
    }

    void Lighting::SetLightTextures(RenderGraph &graph) {
        graph.AddPass("SetLightTextures")
            .Build([&](PassBuilder &builder) {
                builder.Write("LightState", Access::HostWrite);
            })
            .Execute([this](Resources &resources, DeviceContext &device) {
                for (size_t i = 0; i < lights.size() && i < MAX_LIGHTS; i++) {
                    if (lights[i].filterTexture.has_value()) {
                        if (starts_with(lights[i].filterName, "graph:")) {
                            gpuData.lights[i].filterId = scene.textureCache[lights[i].filterName].index;
                        } else {
                            gpuData.lights[i].filterId = lights[i].filterTexture.value();
                        }
                    }
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

        graph.AddPass("RenderMask")
            .Build([&](rg::PassBuilder &builder) {
                ImageDesc desc;
                auto extent = glm::max(glm::ivec2(1), shadowAtlasSize);
                desc.extent = vk::Extent3D(extent.x, extent.y, 1);

                desc.format = vk::Format::eR32Sfloat;
                AttachmentInfo attachmentInfo(LoadOp::Clear, StoreOp::Store);
                attachmentInfo.SetClearColor(glm::vec4(1));
                builder.OutputColorAttachment(0, "Linear", desc, attachmentInfo);

                desc.format = vk::Format::eD16Unorm;
                builder.OutputDepthAttachment("Depth", desc, {LoadOp::Clear, StoreOp::Store});

                builder.ReadPreviousFrame("Linear", Access::FragmentShaderSampleImage);
                builder.ReadPreviousFrame("LightState", Access::FragmentShaderReadUniform);
                builder.ReadUniform("LightState");
            })
            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "shadow_map_mask.frag");

                cmd.SetShaderConstant(ShaderStage::Fragment, "SHADOW_MAP_SAMPLE_WIDTH", CVarShadowMapSampleWidth.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, "SHADOW_MAP_SAMPLE_COUNT", CVarShadowMapSampleCount.Get());

                auto lastFrameID = resources.GetID("ShadowMap.Linear", false, 1);
                if (lastFrameID != InvalidResource) {
                    cmd.SetImageView("shadowMap", lastFrameID);
                } else {
                    cmd.SetImageView("shadowMap", scene.textures.GetSinglePixel(glm::vec4(1)));
                }

                auto lastStateID = resources.GetID("LightState", false, 1);
                if (lastStateID != InvalidResource) {
                    cmd.SetUniformBuffer("PreviousLightData", lastStateID);
                } else {
                    cmd.SetUniformBuffer("PreviousLightData", "LightState");
                }

                cmd.SetUniformBuffer("LightData", "LightState");
                cmd.SetYDirection(YDirection::Down);

                struct {
                    uint32_t lightIndex;
                } constants;

                for (uint32_t i = 0; i < lights.size(); i++) {
                    vk::Rect2D viewport;
                    viewport.extent = vk::Extent2D(views[i].extents.x, views[i].extents.y);
                    viewport.offset = vk::Offset2D(views[i].offset.x, views[i].offset.y);
                    cmd.SetViewport(viewport);

                    constants.lightIndex = i;
                    cmd.PushConstants(constants);

                    cmd.Draw(3); // vertices are defined as constants in the vertex shader
                }
            });

        graph.AddPass("RenderDepth")
            .Build([&](rg::PassBuilder &builder) {
                builder.SetColorAttachment(0, "Linear", {LoadOp::Load, StoreOp::Store});

                builder.SetDepthAttachment("Depth", {LoadOp::Load, StoreOp::Store});

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawAllIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawAllIDs.drawParamsBuffer, Access::VertexShaderReadStorage);
            })
            .Execute([this, drawAllIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("shadow_map.vert", "shadow_map.frag");

                for (uint32_t i = 0; i < lights.size(); i++) {
                    GPUViewState lightViews[] = {{views[i]}, {}};
                    cmd.UploadUniformData("ViewStates", lightViews, 2);

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
                    cmd.UploadUniformData("ViewStates", lightViews, 2);
                    cmd.SetStorageBuffer("OpticVisibility", visBuffer);

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
                        if (visible == 1) {
                            Assertf(opticIndex < optics.size(), "Optic index out of range");
                        } else if (visible != 0) {
                            Abortf("OpticVisibilityReadback got unexpected value: %u", visible);
                        }
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
                        if (*vLight.opticIndex >= MAX_OPTICS ||
                            visibility[*vLight.parentIndex][*vLight.opticIndex] != 1) {
                            lightValid[lightIndex] = false;
                            continue;
                        }
                    }

                    // Check if any optics are visible from the end of the current light path.
                    for (uint32_t opticIndex = 0; opticIndex < optics.size() && opticIndex < MAX_OPTICS; opticIndex++) {
                        if (readbackLights.size() >= MAX_LIGHTS) break;
                        if (vLight.opticIndex && opticIndex == *vLight.opticIndex) continue;
                        if (visibility[lightIndex][opticIndex] == 1) {
                            if (optics[opticIndex].pass) {
                                auto &newLight = readbackLights.emplace_back(vLight);
                                newLight.parentIndex = lightIndex;
                                newLight.opticIndex = opticIndex;
                                newLight.lightPath.emplace_back(optics[opticIndex].ent, false);
                            }
                            if (optics[opticIndex].reflect) {
                                auto &newLight = readbackLights.emplace_back(vLight);
                                newLight.parentIndex = lightIndex;
                                newLight.opticIndex = opticIndex;
                                newLight.lightPath.emplace_back(optics[opticIndex].ent, true);
                            }
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
        auto shadowDepth = CVarBlurShadowMap.Get() ? "ShadowMapBlur.LastOutput" : "ShadowMap.Linear";
        uint32_t voxelLayerCount = std::min(CVarLightingVoxelLayers.Get(), voxels.GetLayerCount());

        graph.AddPass("Lighting")
            .Build([&](rg::PassBuilder &builder) {
                auto gBuffer0 = builder.Read("GBuffer0", Access::FragmentShaderSampleImage);
                builder.Read("GBuffer1", Access::FragmentShaderSampleImage);
                builder.Read("GBuffer2", Access::FragmentShaderSampleImage);
                builder.Read(shadowDepth, Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Radiance", Access::FragmentShaderSampleImage);
                builder.Read("Voxels.Normals", Access::FragmentShaderSampleImage);

                for (auto &voxelLayer : Voxels::VoxelLayers[voxelLayerCount - 1]) {
                    builder.Read(voxelLayer.fullName, Access::FragmentShaderSampleImage);
                }

                auto desc = builder.DeriveImage(gBuffer0);
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(0, "LinearLuminance", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ReadUniform("VoxelState");
                builder.Read("ExposureState", Access::FragmentShaderReadStorage);
                builder.ReadUniform("ViewState");
                builder.ReadUniform("LightState");

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::ReadOnly});
            })
            .Execute([this, shadowDepth, voxelLayerCount](rg::Resources &resources, CommandContext &cmd) {
                if (CVarPCF.Get()) {
                    cmd.SetShaders("screen_cover.vert", "lighting_pcf.frag");
                } else {
                    cmd.SetShaders("screen_cover.vert", "lighting.frag");
                }
                cmd.SetShaderConstant(ShaderStage::Fragment, "MODE", CVarLightingMode.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, "VOXEL_LAYERS", voxelLayerCount);
                cmd.SetShaderConstant(ShaderStage::Fragment, "SHADOW_MAP_SAMPLE_WIDTH", CVarShadowMapSampleWidth.Get());
                cmd.SetShaderConstant(ShaderStage::Fragment, "SHADOW_MAP_SAMPLE_COUNT", CVarShadowMapSampleCount.Get());

                cmd.SetStencilTest(true);
                cmd.SetDepthTest(false, false);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetImageView("gBuffer0", "GBuffer0");
                cmd.SetImageView("gBuffer1", "GBuffer1");
                cmd.SetImageView("gBuffer2", "GBuffer2");
                cmd.SetImageView("gBufferDepth", resources.GetImageDepthView("GBufferDepthStencil"));
                cmd.SetImageView("shadowMap", shadowDepth);
                cmd.SetImageView("voxelRadiance", "Voxels.Radiance");
                cmd.SetImageView("voxelNormals", "Voxels.Normals");

                cmd.SetUniformBuffer("VoxelStateUniform", "VoxelState");
                cmd.SetStorageBuffer("ExposureState", "ExposureState");
                cmd.SetUniformBuffer("ViewStates", "ViewState");
                cmd.SetUniformBuffer("LightData", "LightState");

                cmd.SetBindlessDescriptors(1, scene.textures.GetDescriptorSet());

                for (auto &voxelLayer : Voxels::VoxelLayers[voxelLayerCount - 1]) {
                    cmd.SetImageView(2, voxelLayer.dirIndex, resources.GetImageView(voxelLayer.fullName));
                }
                // cmd.SetBindlessDescriptors(2, voxels.GetCurrentVoxelDescriptorSet());

                cmd.Draw(3);
            });
    }

    bool Lighting::VirtualLight::operator==(const VirtualLight &other) const {
        return source == other.source && lightPath.size() == other.lightPath.size() &&
               std::equal(lightPath.begin(), lightPath.end(), other.lightPath.begin());
    }
} // namespace sp::vulkan::renderer
