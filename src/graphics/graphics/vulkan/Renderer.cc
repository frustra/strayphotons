/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Renderer.hh"

#include "ecs/EcsImpl.hh"
#include "ecs/components/Renderable.hh"
#include "game/Game.hh"
#include "game/SceneManager.hh"
#include "graphics/vulkan/Compositor.hh"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Image.hh"
#include "graphics/vulkan/core/Util.hh"
#include "graphics/vulkan/render_graph/PassBuilder.hh"
#include "graphics/vulkan/render_graph/PooledImage.hh"
#include "graphics/vulkan/render_graph/Resources.hh"
#include "graphics/vulkan/render_passes/Bloom.hh"
#include "graphics/vulkan/render_passes/Crosshair.hh"
#include "graphics/vulkan/render_passes/Exposure.hh"
#include "graphics/vulkan/render_passes/LightSensors.hh"
#include "graphics/vulkan/render_passes/MarchingCubes.hh"
#include "graphics/vulkan/render_passes/Outline.hh"
#include "graphics/vulkan/render_passes/Skybox.hh"
#include "graphics/vulkan/render_passes/Tonemap.hh"
#include "graphics/vulkan/render_passes/VisualizeBuffer.hh"
#include "graphics/vulkan/scene/GPUScene.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"
#include "gui/GuiContext.hh"
#include "strayphotons/Logging.hh"
#include "strayphotons/Utility.hh"
#include "vulkan/vulkan.hpp"
#include "xr/XrSystem.hh"

#include <string>
#include <vector>

namespace sp::vulkan {
    static const std::string defaultWindowViewTarget = "gui:menu";
    static const std::string defaultXrViewTarget = "XrView";

    CVar<std::string> CVarWindowViewTarget("r.WindowView", defaultWindowViewTarget, "Primary window's render target");

    static CVar<bool> CVarMirrorXR("r.MirrorXR", false, "Mirror XR in primary window");

    static CVar<uint32_t> CVarWindowViewTargetLayer("r.WindowViewTargetLayer", 0, "Array layer to view");

    static CVar<std::string> CVarXrViewTarget("r.XrView", defaultXrViewTarget, "HMD's render target");

    static CVar<bool> CVarSMAA("r.SMAA", true, "Enable SMAA");

    static CVar<bool> CVarSortedDraw("r.SortedDraw", true, "Draw geometry in sorted depth-order");
    static CVar<bool> CVarDrawReverseOrder("r.DrawReverseOrder", false, "Flip the order for geometry depth sorting");

    Renderer::Renderer(Game &game, DeviceContext &device, rg::RenderGraph &graph, Compositor &compositor)
        : game(game), device(device), graph(graph), compositor(compositor), scene(device), voxels(scene),
          marchingCubes(scene), lighting(scene, voxels), transparency(scene, voxels), emissive(scene) {
        funcs.Register("listgraphimages", "List all images in the render graph", [&]() {
            listImages = true;
        });

        depthStencilFormat = device.SelectSupportedFormat(vk::FormatFeatureFlagBits::eDepthStencilAttachment,
            {vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint, vk::Format::eD16UnormS8Uint});
    }

    Renderer::~Renderer() {
        if (!device.RequiresReset()) device->waitIdle();
    }

    void Renderer::AttachWindow(const std::shared_ptr<GuiContext> &context) {
        windowGuiContext = context;
    }

    void Renderer::RenderFrame(chrono_clock::duration elapsedTime) {
        if (CVarMirrorXR.Changed()) {
            bool mirrorXR = CVarMirrorXR.Get(true);
            CVarWindowViewTarget.Set(mirrorXR ? CVarXrViewTarget.Get() : defaultWindowViewTarget);
        }

        if (game.xr) game.xr->WaitFrame();

        graph.AddImageView("ErrorColor", scene.textures.GetSinglePixel(ERROR_COLOR));

        compositor.BeforeFrame(graph);
        activeGuiContext = windowGuiContext.lock();
        if (activeGuiContext) activeGuiContext->BeforeFrame(compositor);

        BuildFrameGraph(elapsedTime);

        CVarWindowViewTarget.UpdateCompletions([&](std::vector<std::string> &completions) {
            auto list = graph.AllImages();
            for (const auto &info : list) {
                completions.emplace_back(info.name.data(), info.name.size());
            }
        });

        CVarXrViewTarget.UpdateCompletions([&](std::vector<std::string> &completions) {
            auto list = graph.AllImages();
            for (const auto &info : list) {
                completions.emplace_back(info.name.data(), info.name.size());
            }
        });

        if (listImages) {
            listImages = false;
            auto list = graph.AllImages();
            for (const auto &info : list) {
                auto &extent = info.desc.extent;
                Logf("%s (%dx%dx%d [%d] %s)",
                    info.name,
                    extent.width,
                    extent.height,
                    extent.depth,
                    info.desc.arrayLayers,
                    vk::to_string(info.desc.format));
            }
        }

        graph.Execute();
    }

    void Renderer::BuildFrameGraph(chrono_clock::duration elapsedTime) {
        ZoneScoped;

        compositor.AddOutputPasses(Compositor::PassOrder::BeforeViews);
        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name,
                ecs::FocusLock,
                ecs::GuiElement,
                ecs::LaserLine,
                ecs::Light,
                ecs::LightSensor,
                ecs::OpticalElement,
                ecs::Renderable,
                ecs::RenderOutput,
                ecs::Screen,
                ecs::Scripts,
                ecs::TransformSnapshot,
                ecs::View,
                ecs::VoxelArea,
                ecs::XrView>>();

            scene.LoadState(graph, lock);
            lighting.LoadState(graph, lock);
            voxels.LoadState(graph, lock);

            scene.AddGeometryWarp(graph);
            lighting.AddShadowPasses(graph);
            scene.textures.AddGraphTextures(graph);
            voxels.AddVoxelizationInit(graph, lighting);
            voxels.AddVoxelization(graph, lighting);
            voxels.AddVoxelization2(graph, lighting);
            marchingCubes.AddMarchingCubes(graph, voxels);
            renderer::AddLightSensors(graph, scene, lock);

            AddViewOutputs(lock, elapsedTime);

            Assert(lock.UseCount() == 1, "something held onto the renderer lock");
        }
        compositor.AddOutputPasses(Compositor::PassOrder::AfterViews);
        screenshots.AddPass(graph);
        AddWindowOutput();
    }

    void Renderer::AddWindowOutput() {
        auto swapchainImage = device.SwapchainImageView();
        if (!swapchainImage) return;

        if (!logoTex) {
            logoTex = device.LoadAssetImage("logos/splash.png", true);
            device.FlushMainQueue();
        }

        graph.AddImageView("WindowFinalOutput", swapchainImage);

        rg::ResourceID guiResourceID = rg::InvalidResource;
        if (activeGuiContext) {
            auto windowScale = CVarWindowScale.Get();
            if (windowScale.x <= 0.0f) windowScale.x = 1.0f;
            if (windowScale.y <= 0.0f) windowScale.y = windowScale.x;

            guiResourceID = graph.AddPass("WindowOverlay")
                                .Build([&](rg::PassBuilder &builder) {
                                    rg::ImageDesc overlayDesc = {};
                                    overlayDesc.extent = swapchainImage->Extent();
                                    overlayDesc.format = swapchainImage->Format();
                                    builder.OutputColorAttachment(0,
                                        "WindowOverlay",
                                        overlayDesc,
                                        {LoadOp::Clear, StoreOp::Store});
                                })
                                .Execute([](rg::Resources &resources, CommandContext &cmd) {});

            auto extent = swapchainImage->Extent();
            compositor.DrawGuiContext(*activeGuiContext, glm::ivec4{0, 0, extent.width, extent.height}, windowScale);
        }

        rg::ResourceID sourceID = rg::InvalidResource;
        graph.AddPass("WindowOutput")
            .Build([&](rg::PassBuilder &builder) {
                builder.RequirePass();

                rg::ResourceName sourceName = CVarWindowViewTarget.Get();
                if (!sourceName.empty() && !starts_with(sourceName, "/")) sourceName = "/" + sourceName;
                sourceID = builder.GetID(sourceName, false);
                if (sourceID == rg::InvalidResource && sourceName != "/" + defaultWindowViewTarget) {
                    Errorf("image %s does not exist, defaulting to %s", sourceName, defaultWindowViewTarget);
                    CVarWindowViewTarget.Set(defaultWindowViewTarget);
                    sourceID = builder.GetID("/" + defaultWindowViewTarget, false);
                }

                auto loadOp = LoadOp::DontCare;

                if (sourceID != rg::InvalidResource) {
                    auto res = builder.GetResource(sourceID);
                    auto format = res.ImageFormat();
                    auto layer = CVarWindowViewTargetLayer.Get();
                    if (FormatComponentCount(format) != 4 || FormatByteSize(format) != 4 || layer != 0) {
                        sourceID = renderer::VisualizeBuffer(graph, res.id, layer);
                    }
                    builder.Read(sourceID, Access::FragmentShaderSampleImage);
                } else {
                    loadOp = LoadOp::Clear;
                }
                if (guiResourceID != rg::InvalidResource) {
                    builder.Read(guiResourceID, Access::FragmentShaderSampleImage);
                }
                builder.SetColorAttachment(0, "WindowFinalOutput", {loadOp, StoreOp::Store});
            })
            .Execute([this, sourceID, guiResourceID](rg::Resources &resources, CommandContext &cmd) {
                if (sourceID != rg::InvalidResource) {
                    auto view = resources.GetImageView(sourceID);
                    cmd.DrawScreenCover(view);
                    if (view) device.SetOutputExtents(view->GetWidth(), view->GetHeight());
                } else if (logoTex->Ready()) {
                    cmd.SetBlending(true);
                    cmd.DrawScreenCover(logoTex->Get());
                }
                if (guiResourceID != rg::InvalidResource) {
                    cmd.SetBlending(true);
                    // GUI outputs premultiplied alpha
                    cmd.SetBlendFunc(vk::BlendFactor::eOne,
                        vk::BlendFactor::eOneMinusSrcAlpha,
                        vk::BlendFactor::eOneMinusDstAlpha,
                        vk::BlendFactor::eOne);
                    auto view = resources.GetImageView(guiResourceID);
                    if (view) cmd.DrawScreenCover(view);
                }
            });
    }

    void Renderer::AddViewOutputs(ecs::Lock<ecs::Read<ecs::Name,
                                      ecs::TransformSnapshot,
                                      ecs::View,
                                      ecs::Screen,
                                      ecs::LaserLine,
                                      ecs::GuiElement,
                                      ecs::XrView>> lock,
        chrono_clock::duration elapsedTime) {
        ZoneScoped;
        for (auto &ent : lock.EntitiesWith<ecs::View>()) {
            auto entityScope = graph.Scope(ent.Get<ecs::Name>(lock).String());
            auto viewScope = graph.Scope("View");
            auto view = AddFlatView(lock, ent);
            if (view) {
                AddDeferredPasses(lock, view, elapsedTime);
                renderer::AddCrosshair(graph); // TODO: Move to HUD gui effects
            }
        }

        if (game.xr) {
            {
                auto scope = graph.Scope("XrView");
                auto view = AddXrView(lock);
                if (graph.HasResource("GBuffer0") && view) AddDeferredPasses(lock, view, elapsedTime);
            }
            AddXrSubmit(lock);
        }
    }

    ecs::View Renderer::AddFlatView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View>> lock, ecs::Entity ent) {
        if (!ent || !ent.Has<ecs::View>(lock)) return {};

        auto view = ent.Get<ecs::View>(lock);
        if (!view) return {};
        view.UpdateViewMatrix(lock, ent);

        GPUScene::DrawBufferIDs drawIDs;
        if (CVarSortedDraw.Get()) {
            glm::vec3 viewPos = view.invViewMat * glm::vec4(0, 0, 0, 1);
            drawIDs = scene.GenerateSortedDrawsForView(graph, viewPos, view.visibilityMask, CVarDrawReverseOrder.Get());
        } else {
            drawIDs = scene.GenerateDrawsForView(graph, view.visibilityMask);
        }
        graph.AddPass("UpdateView")
            .Build([&](rg::PassBuilder &builder) {
                builder.CreateUniform("ViewState", sizeof(GPUViewState) * 2);
            })
            .Execute([view](rg::Resources &resources, DeviceContext &device) {
                GPUViewState viewState[] = {{view}, {}};
                auto viewStateBuf = resources.GetBuffer("ViewState");
                viewStateBuf->CopyFrom(viewState, 2);
            });
        if (!drawIDs) return view;

        graph.AddPass("ForwardPass")
            .Build([&](rg::PassBuilder &builder) {
                rg::ImageDesc desc = {};
                desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);
                desc.primaryViewType = vk::ImageViewType::e2DArray;

                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "GBuffer0", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(1, "GBuffer1", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR8Unorm;
                builder.OutputColorAttachment(2, "GBuffer2", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = depthStencilFormat;
                builder.OutputDepthAttachment("GBufferDepthStencil", desc, {LoadOp::Clear, StoreOp::Store});

                builder.Read("ViewState", Access::VertexShaderReadUniform);

                builder.ReadPreviousFrame("/MarchingCubes/VertexBuffer", Access::VertexBuffer);
                builder.ReadPreviousFrame("/MarchingCubes/IndexBuffer", Access::IndexBuffer);
                builder.ReadPreviousFrame("/MarchingCubes/IndexBuffer", Access::IndirectBuffer);

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawIDs.drawParamsBuffer, Access::VertexShaderReadStorage);
            })
            .Execute([this, drawIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "generate_gbuffer.frag");

                cmd.SetUniformBuffer("ViewStates", "ViewState");

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawIDs.drawCommandsBuffer),
                    resources.GetBuffer(drawIDs.drawParamsBuffer));

                auto vertexID = resources.GetID("MarchingCubes/VertexBuffer", 1);
                auto indexID = resources.GetID("MarchingCubes/IndexBuffer", 1);
                auto vertexBuffer = resources.GetBuffer(vertexID);
                auto indexBuffer = resources.GetBuffer(indexID);
                if (vertexBuffer && indexBuffer) {
                    cmd.Raw().bindIndexBuffer(*indexBuffer,
                        sizeof(VkDrawIndexedIndirectCommand),
                        vk::IndexType::eUint32);
                    cmd.Raw().bindVertexBuffers(0, {*vertexBuffer}, {0});
                    cmd.DrawIndexedIndirect(indexBuffer, 0u, 1u);
                }
            });
        return view;
    }

    ecs::View Renderer::AddXrView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View, ecs::XrView>> lock) {
        if (!game.xr) return {};

        auto xrViews = lock.EntitiesWith<ecs::XrView>();
        if (xrViews.size() == 0) return {};

        glm::ivec2 viewExtents = glm::ivec2(0);
        sp::EnumArray<ecs::View, ecs::XrEye> viewsByEye;

        for (const ecs::Entity &ent : xrViews) {
            if (!ent.Has<ecs::View>(lock)) continue;
            auto &view = ent.Get<ecs::View>(lock);

            if (viewExtents == glm::ivec2(0)) viewExtents = view.extents;
            Assert(viewExtents == view.extents, "All XR views must have the same extents");

            auto &xrView = ent.Get<ecs::XrView>(lock);
            viewsByEye[xrView.eye] = view;
            viewsByEye[xrView.eye].UpdateViewMatrix(lock, ent);
        }

        xrRenderPoses.resize(xrViews.size());

        if (!hiddenAreaMesh[0]) {
            for (size_t i = 0; i < hiddenAreaMesh.size(); i++) {
                auto mesh = game.xr->GetHiddenAreaMesh(ecs::XrEye(i));
                if (mesh.triangleCount == 0) {
                    static const std::array triangle = {glm::vec2(0), glm::vec2(0), glm::vec2(0)};
                    hiddenAreaMesh[i] = device.CreateBuffer(triangle.data(),
                        triangle.size(),
                        vk::BufferUsageFlagBits::eVertexBuffer,
                        VMA_MEMORY_USAGE_CPU_TO_GPU);
                    hiddenAreaTriangleCount[i] = 1;
                } else {
                    hiddenAreaMesh[i] = device.CreateBuffer(mesh.vertices,
                        mesh.triangleCount * 3,
                        vk::BufferUsageFlagBits::eVertexBuffer,
                        VMA_MEMORY_USAGE_CPU_TO_GPU);
                    hiddenAreaTriangleCount[i] = mesh.triangleCount;
                }
            }
        }

        auto executeHiddenAreaStencil = [this](uint32_t eyeIndex) {
            return [this, eyeIndex](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("basic_ortho_stencil.vert", "noop.frag");

                glm::mat4 proj = MakeOrthographicProjection(0, 1, 1, 0);
                cmd.PushConstants(proj);

                cmd.SetCullMode(vk::CullModeFlagBits::eNone);
                cmd.SetDepthTest(false, false);
                cmd.SetStencilTest(true);
                cmd.SetStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilCompareOp(vk::CompareOp::eAlways);
                cmd.SetStencilPassOp(vk::StencilOp::eReplace);
                cmd.SetStencilFailOp(vk::StencilOp::eReplace);
                cmd.SetStencilDepthFailOp(vk::StencilOp::eReplace);

                cmd.SetVertexLayout(PositionVertex2D::Layout());
                cmd.Raw().bindVertexBuffers(0, {*this->hiddenAreaMesh[eyeIndex]}, {0});
                cmd.Draw(this->hiddenAreaTriangleCount[eyeIndex] * 3);
            };
        };

        graph.AddPass("HiddenAreaStencil0")
            .Build([&](rg::PassBuilder &builder) {
                rg::ImageDesc desc = {};
                desc.extent = vk::Extent3D(viewExtents.x, viewExtents.y, 1);
                desc.arrayLayers = xrViews.size();
                desc.primaryViewType = vk::ImageViewType::e2DArray;
                desc.format = depthStencilFormat;
                rg::AttachmentInfo attachment = {LoadOp::Clear, StoreOp::Store};
                attachment.arrayIndex = 0;
                builder.OutputDepthAttachment("GBufferDepthStencil", desc, attachment);
            })
            .Execute(executeHiddenAreaStencil(0));

        graph.AddPass("HiddenAreaStencil1")
            .Build([&](rg::PassBuilder &builder) {
                rg::AttachmentInfo attachment = {LoadOp::Clear, StoreOp::Store};
                attachment.arrayIndex = 1;
                builder.SetDepthAttachment("GBufferDepthStencil", attachment);
            })
            .Execute(executeHiddenAreaStencil(1));

        glm::vec3 viewPos = viewsByEye[ecs::XrEye::Left].invViewMat * glm::vec4(0, 0, 0, 1);
        auto drawIDs = scene.GenerateSortedDrawsForView(graph, viewPos, viewsByEye[ecs::XrEye::Left].visibilityMask);
        if (!drawIDs) return {};

        graph.AddPass("ForwardPass")
            .Build([&](rg::PassBuilder &builder) {
                rg::ImageDesc desc = {};
                desc.extent = vk::Extent3D(viewExtents.x, viewExtents.y, 1);
                desc.arrayLayers = xrViews.size();
                desc.primaryViewType = vk::ImageViewType::e2DArray;

                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "GBuffer0", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR16G16B16A16Sfloat;
                builder.OutputColorAttachment(1, "GBuffer1", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR8Unorm;
                builder.OutputColorAttachment(2, "GBuffer2", desc, {LoadOp::Clear, StoreOp::Store});

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});

                builder.CreateUniform("ViewState", sizeof(GPUViewState) * viewsByEye.size());

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawIDs.drawParamsBuffer, Access::VertexShaderReadStorage);
            })
            .Execute([this, viewsByEye, drawIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "generate_gbuffer.frag");

                cmd.SetStencilTest(true);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                auto viewStateBuf = resources.GetBuffer("ViewState");
                cmd.SetUniformBuffer("ViewStates", viewStateBuf);

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawIDs.drawCommandsBuffer),
                    resources.GetBuffer(drawIDs.drawParamsBuffer));

                GPUViewState *viewState;
                viewStateBuf->Map((void **)&viewState);
                for (auto &eye : magic_enum::enum_values<ecs::XrEye>()) {
                    auto view = viewsByEye[eye];
                    auto i = (size_t)eye;

                    if (this->game.xr && this->game.xr->GetPredictedViewPose(eye, this->xrRenderPoses[i])) {
                        view.SetInvViewMat(view.invViewMat * this->xrRenderPoses[i]);
                    }

                    viewState[i] = GPUViewState(view);
                }
                viewStateBuf->Unmap();
                viewStateBuf->Flush();
            });
        return viewsByEye[ecs::XrEye::Left];
    }

    void Renderer::AddXrSubmit(ecs::Lock<ecs::Read<ecs::XrView>> lock) {
        if (!game.xr) return;

        auto xrViews = lock.EntitiesWith<ecs::XrView>();
        if (xrViews.size() != 2) return;

        rg::ResourceID sourceID;
        graph.AddPass("XrSubmit")
            .Build([&](rg::PassBuilder &builder) {
                rg::ResourceName sourceName = CVarXrViewTarget.Get();
                if (!sourceName.empty() && !starts_with(sourceName, "/")) sourceName = "/" + sourceName;
                sourceID = builder.GetID(sourceName, false);
                if (sourceID == rg::InvalidResource && sourceName != "/" + defaultXrViewTarget) {
                    Errorf("image %s does not exist, defaulting to %s", sourceName, defaultXrViewTarget);
                    CVarXrViewTarget.Set(defaultXrViewTarget);
                    sourceID = builder.GetID("/" + defaultXrViewTarget, false);
                }

                if (sourceID != rg::InvalidResource) {
                    auto res = builder.GetResource(sourceID);
                    auto format = res.ImageFormat();
                    if (FormatComponentCount(format) != 4 || FormatByteSize(format) != 4) {
                        sourceID = renderer::VisualizeBuffer(graph, res.id);
                    }
                    builder.Read(sourceID, Access::TransferRead);
                }
                builder.FlushCommands();
                builder.RequirePass();
            })
            .Execute([this, sourceID](rg::Resources &resources, DeviceContext &device) {
                if (!this->game.xr) return;
                auto xrImage = resources.GetImageView(sourceID);

                for (size_t i = 0; i < 2; i++) {
                    this->game.xr->SubmitView(ecs::XrEye(i), this->xrRenderPoses[i], xrImage.get());
                }
            });
    }

    void Renderer::AddDeferredPasses(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Screen, ecs::LaserLine>> lock,
        const ecs::View &view,
        chrono_clock::duration elapsedTime) {
        renderer::AddExposureState(graph);
        if (graph.HasResource("GBuffer0")) {
            lighting.AddLightingPass(graph);
        } else {
            // Create blank gbuffer to run postprocessing on
            graph.AddPass("EmptyFrame")
                .Build([&](rg::PassBuilder &builder) {
                    rg::ImageDesc desc = {};
                    desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);
                    desc.primaryViewType = vk::ImageViewType::e2DArray;
                    desc.format = vk::Format::eR16G16B16A16Sfloat;
                    builder.OutputColorAttachment(0, "LinearLuminance", desc, {LoadOp::Clear, StoreOp::Store});
                    desc.format = depthStencilFormat;
                    builder.OutputDepthAttachment("GBufferDepthStencil", desc, {LoadOp::Clear, StoreOp::Store});
                })
                .Execute([](rg::Resources &resources, CommandContext &cmd) {});
        }
        renderer::AddSkyboxPass(graph);
        if (graph.HasResource("GBuffer0")) {
            transparency.AddPass(graph, view);
        }
        emissive.AddPass(graph, lock, elapsedTime);
        voxels.AddDebugPass(graph);
        renderer::AddExposureUpdate(graph);
        renderer::AddOutlines(graph, scene);
        renderer::AddBloom(graph);
        renderer::AddTonemap(graph);

        if (CVarSMAA.Get()) smaa.AddPass(graph);
    }

    void Renderer::EndFrame() {
        ZoneScoped;
        compositor.EndFrame();

        GetSceneManager().PreloadSceneGraphics([&](auto lock, auto scene) {
            ZoneScopedN("PreloadSceneGraphics");
            bool complete = true;
            if (!this->scene.PreloadScene(lock, scene)) complete = false;
            if (!smaa.PreloadTextures(device)) complete = false;
            return complete;
        });

        scene.Flush();
    }
} // namespace sp::vulkan
