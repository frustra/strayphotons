/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Renderer.hh"

#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "game/Game.hh"
#include "game/SceneManager.hh"
#include "graphics/gui/MenuGuiManager.hh"
#include "graphics/gui/ScriptGuiManager.hh"
#include "graphics/gui/WorldGuiManager.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Image.hh"
#include "graphics/vulkan/core/Util.hh"
#include "graphics/vulkan/core/VkTracing.hh"
#include "graphics/vulkan/gui/GuiRenderer.hh"
#include "graphics/vulkan/render_passes/Bloom.hh"
#include "graphics/vulkan/render_passes/Blur.hh"
#include "graphics/vulkan/render_passes/Crosshair.hh"
#include "graphics/vulkan/render_passes/Exposure.hh"
#include "graphics/vulkan/render_passes/LightSensors.hh"
#include "graphics/vulkan/render_passes/Mipmap.hh"
#include "graphics/vulkan/render_passes/Outline.hh"
#include "graphics/vulkan/render_passes/Skybox.hh"
#include "graphics/vulkan/render_passes/Tonemap.hh"
#include "graphics/vulkan/render_passes/VisualizeBuffer.hh"
#include "graphics/vulkan/scene/Mesh.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"
#include "xr/XrSystem.hh"

#include <assets/AssetManager.hh>

namespace sp::vulkan {
    static const std::string defaultWindowViewTarget = "FlatView.LastOutput";
    static const std::string defaultXrViewTarget = "XrView.LastOutput";

    CVar<string> CVarWindowViewTarget("r.WindowView", defaultWindowViewTarget, "Primary window's render target");

    static CVar<bool> CVarMirrorXR("r.MirrorXR", false, "Mirror XR in primary window");

    static CVar<uint32> CVarWindowViewTargetLayer("r.WindowViewTargetLayer", 0, "Array layer to view");

    static CVar<string> CVarXrViewTarget("r.XrView", defaultXrViewTarget, "HMD's render target");

    static CVar<bool> CVarSMAA("r.SMAA", true, "Enable SMAA");

    static CVar<bool> CVarSortedDraw("r.SortedDraw", true, "Draw geometry in sorted depth-order");
    static CVar<bool> CVarDrawReverseOrder("r.DrawReverseOrder", false, "Flip the order for geometry depth sorting");

    Renderer::Renderer(Game &game, DeviceContext &device)
        : game(game), device(device), graph(device), scene(device), voxels(scene), lighting(scene, voxels),
          transparency(scene, voxels), emissive(scene), guiRenderer(new GuiRenderer(device, scene)) {
        funcs.Register("listgraphimages", "List all images in the render graph", [&]() {
            listImages = true;
        });

        auto lock = ecs::StartTransaction<ecs::AddRemove>();
        guiObserver = lock.Watch<ecs::ComponentAddRemoveEvent<ecs::Gui>>();
        renderableObserver = lock.Watch<ecs::ComponentModifiedEvent<ecs::Renderable>>();
        lightObserver = lock.Watch<ecs::ComponentModifiedEvent<ecs::Light>>();

        for (auto &ent : lock.EntitiesWith<ecs::Gui>()) {
            const ecs::Scripts *scripts = nullptr;
            if (ent.Has<ecs::Scripts>(lock)) scripts = &ent.Get<ecs::Scripts>(lock);
            AddGui(ent, ent.Get<const ecs::Gui>(lock), scripts);
        }

        depthStencilFormat = device.SelectSupportedFormat(vk::FormatFeatureFlagBits::eDepthStencilAttachment,
            {vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint, vk::Format::eD16UnormS8Uint});
    }

    Renderer::~Renderer() {
        if (!device.RequiresReset()) device->waitIdle();

        {
            auto lock = ecs::StartTransaction<ecs::AddRemove>();
            guiObserver.Stop(lock);
            renderableObserver.Stop(lock);
            lightObserver.Stop(lock);
        }
    }

    void Renderer::RenderFrame(chrono_clock::duration elapsedTime) {
        if (CVarMirrorXR.Changed()) {
            bool mirrorXR = CVarMirrorXR.Get(true);
            CVarWindowViewTarget.Set(mirrorXR ? CVarXrViewTarget.Get() : defaultWindowViewTarget);
        }

        if (game.xr) game.xr->WaitFrame();

        for (auto &gui : guis) {
            if (gui.contextShared) gui.contextShared->BeforeFrame();
        }

        BuildFrameGraph(elapsedTime);

        CVarWindowViewTarget.UpdateCompletions([&](vector<string> &completions) {
            auto list = graph.AllImages();
            for (const auto &info : list) {
                completions.emplace_back(info.name.data(), info.name.size());
            }
        });

        CVarXrViewTarget.UpdateCompletions([&](vector<string> &completions) {
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
        auto lock = ecs::StartTransaction<ecs::Read<ecs::Name,
            ecs::TransformSnapshot,
            ecs::LaserLine,
            ecs::Light,
            ecs::LightSensor,
            ecs::VoxelArea,
            ecs::Renderable,
            ecs::View,
            ecs::XrView,
            ecs::OpticalElement,
            ecs::Gui,
            ecs::Screen,
            ecs::Scripts,
            ecs::FocusLock>>();

        scene.LoadState(graph, lock);
        lighting.LoadState(graph, lock);
        voxels.LoadState(graph, lock);

        scene.AddGeometryWarp(graph);
        lighting.AddShadowPasses(graph);
        AddWorldGuis(lock);
        AddMenuGui(lock);
        scene.AddGraphTextures(graph);
        lighting.SetLightTextures(graph);
        voxels.AddVoxelizationInit(graph, lighting);
        voxels.AddVoxelization(graph, lighting);
        voxels.AddVoxelization2(graph, lighting);
        renderer::AddLightSensors(graph, scene, lock);

        if (game.xr) {
            {
                auto scope = graph.Scope("XrView");
                auto view = AddXrView(lock);
                if (graph.HasResource("GBuffer0") && view) AddDeferredPasses(lock, view, elapsedTime);
            }
            AddXrSubmit(lock);
        }

        {
            auto scope = graph.Scope("FlatView");
            auto view = AddFlatView(lock);
            if (graph.HasResource("GBuffer0") && view) {
                AddDeferredPasses(lock, view, elapsedTime);
                renderer::AddCrosshair(graph);
            } else {
                if (!logoTex) logoTex = device.LoadAssetImage(sp::Assets().LoadImage("logos/splash.png")->Get(), true);
                graph.AddPass("LogoOverlay")
                    .Build([](rg::PassBuilder &builder) {
                        rg::ImageDesc desc;
                        auto windowSize = CVarWindowSize.Get();
                        auto windowScale = CVarWindowScale.Get();
                        if (windowScale.x <= 0.0f) windowScale.x = 1.0f;
                        if (windowScale.y <= 0.0f) windowScale.y = windowScale.x;
                        desc.extent = vk::Extent3D(windowSize.x / windowScale.x, windowSize.y / windowScale.y, 1);
                        desc.format = vk::Format::eR8G8B8A8Srgb;
                        builder.OutputColorAttachment(0, "LogoView", desc, {LoadOp::DontCare, StoreOp::Store});
                    })
                    .Execute([&](rg::Resources &resources, CommandContext &cmd) {
                        cmd.DrawScreenCover(scene.textures.GetSinglePixel(glm::vec4(0, 0, 0, 1)));
                        if (logoTex->Ready()) {
                            cmd.SetBlending(true);
                            cmd.DrawScreenCover(logoTex->Get());
                        }
                    });
            }
            if (lock.Get<ecs::FocusLock>().HasFocus(ecs::FocusLayer::Menu)) AddMenuOverlay();
        }
        screenshots.AddPass(graph);
        AddWindowOutput();

        Assert(lock.UseCount() == 1, "something held onto the renderer lock");
    }

    void Renderer::AddWindowOutput() {
        auto swapchainImage = device.SwapchainImageView();
        if (!swapchainImage) return;

        rg::ResourceID sourceID = rg::InvalidResource;
        graph.AddPass("WindowFinalOutput")
            .Build([&](rg::PassBuilder &builder) {
                builder.RequirePass();

                auto &sourceName = CVarWindowViewTarget.Get();
                sourceID = builder.GetID(sourceName, false);
                if (sourceID == rg::InvalidResource && sourceName != defaultWindowViewTarget) {
                    Errorf("image %s does not exist, defaulting to %s", sourceName, defaultWindowViewTarget);
                    CVarWindowViewTarget.Set(defaultWindowViewTarget);
                    sourceID = builder.GetID(defaultWindowViewTarget, false);
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

                rg::ImageDesc desc;
                desc.extent = swapchainImage->Extent();
                desc.format = swapchainImage->Format();
                builder.OutputColorAttachment(0, "WindowFinalOutput", desc, {loadOp, StoreOp::Store});
            })
            .Execute([this, sourceID](rg::Resources &resources, CommandContext &cmd) {
                if (sourceID != rg::InvalidResource) {
                    auto source = resources.GetImageView(sourceID);
                    cmd.DrawScreenCover(source);
                }

                if (overlayGui) {
                    auto windowScale = CVarWindowScale.Get();
                    if (windowScale.x <= 0.0f) windowScale.x = 1.0f;
                    if (windowScale.y <= 0.0f) windowScale.y = windowScale.x;

                    guiRenderer->Render(*overlayGui, cmd, vk::Rect2D{{0, 0}, cmd.GetFramebufferExtent()}, windowScale);
                }
            });

        graph.SetTargetImageView("WindowFinalOutput", swapchainImage);
    }

    ecs::View Renderer::AddFlatView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View>> lock) {
        ecs::Entity windowEntity = device.GetActiveView();

        if (!windowEntity || !windowEntity.Has<ecs::View>(lock)) return {};

        auto view = windowEntity.Get<ecs::View>(lock);
        if (!view) return {};
        view.UpdateViewMatrix(lock, windowEntity);

        GPUScene::DrawBufferIDs drawIDs;
        if (CVarSortedDraw.Get()) {
            glm::vec3 viewPos = view.invViewMat * glm::vec4(0, 0, 0, 1);
            drawIDs = scene.GenerateSortedDrawsForView(graph, viewPos, view.visibilityMask, CVarDrawReverseOrder.Get());
        } else {
            drawIDs = scene.GenerateDrawsForView(graph, view.visibilityMask);
        }

        graph.AddPass("ForwardPass")
            .Build([&](rg::PassBuilder &builder) {
                rg::ImageDesc desc;
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

                builder.CreateUniform("ViewState", sizeof(GPUViewState) * 2);
                builder.Read("ViewState", Access::VertexShaderReadUniform);

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawIDs.drawParamsBuffer, Access::VertexShaderReadStorage);
            })
            .Execute([this, view, drawIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "generate_gbuffer.frag");

                GPUViewState viewState[] = {{view}, {}};
                auto viewStateBuf = resources.GetBuffer("ViewState");
                viewStateBuf->CopyFrom(viewState, 2);
                cmd.SetUniformBuffer("ViewStates", viewStateBuf);

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawIDs.drawCommandsBuffer),
                    resources.GetBuffer(drawIDs.drawParamsBuffer));
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

        auto executeHiddenAreaStencil = [this](uint32 eyeIndex) {
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
                rg::ImageDesc desc;
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

        graph.AddPass("ForwardPass")
            .Build([&](rg::PassBuilder &builder) {
                rg::ImageDesc desc;
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
                auto &sourceName = CVarXrViewTarget.Get();
                sourceID = builder.GetID(sourceName, false);
                if (sourceID == rg::InvalidResource && sourceName != defaultXrViewTarget) {
                    Errorf("image %s does not exist, defaulting to %s", sourceName, defaultXrViewTarget);
                    CVarXrViewTarget.Set(defaultXrViewTarget);
                    sourceID = builder.GetID(defaultXrViewTarget, false);
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

    void Renderer::AddGui(ecs::Entity ent, const ecs::Gui &gui, const ecs::Scripts *scripts) {
        ecs::GetScriptManager().WithGuiScriptLock([&] {
            auto window = LookupInternalGui(gui.windowName).lock();
            if (window) {
                auto context = make_shared<WorldGuiManager>(ent, window->name);
                context->Attach(window);
                auto windowScale = CVarWindowScale.Get();
                if (windowScale.x <= 0.0f) windowScale.x = 1.0f;
                if (windowScale.y <= 0.0f) windowScale.y = windowScale.x;
                guis.emplace_back(RenderableGui{ent, windowScale, context.get(), context});
            } else {
                auto scriptState = LookupScriptGui(gui.windowName, scripts);
                if (scriptState) {
                    auto context = make_shared<ScriptGuiManager>(ent, gui.windowName, scriptState);
                    auto windowScale = CVarWindowScale.Get();
                    if (windowScale.x <= 0.0f) windowScale.x = 1.0f;
                    if (windowScale.y <= 0.0f) windowScale.y = windowScale.x;
                    guis.emplace_back(RenderableGui{ent, windowScale, context.get(), context});
                }
            }
        });
    }

    void Renderer::AddWorldGuis(
        ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Gui, ecs::Screen, ecs::Name, ecs::Scripts>> lock) {
        ecs::ComponentAddRemoveEvent<ecs::Gui> guiEvent;
        while (guiObserver.Poll(lock, guiEvent)) {
            auto &eventEntity = guiEvent.entity;

            if (guiEvent.type == Tecs::EventType::REMOVED) {
                for (auto it = guis.begin(); it != guis.end(); it++) {
                    if (it->entity == eventEntity) {
                        guis.erase(it);
                        break;
                    }
                }
            } else if (guiEvent.type == Tecs::EventType::ADDED) {
                if (!eventEntity.Has<ecs::Gui>(lock)) continue;
                const ecs::Scripts *scripts = nullptr;
                if (eventEntity.Has<ecs::Scripts>(lock)) scripts = &eventEntity.Get<ecs::Scripts>(lock);
                AddGui(eventEntity, eventEntity.Get<ecs::Gui>(lock), scripts);
            }
        }

        for (auto &gui : guis) {
            if (!gui.entity.Has<ecs::Gui, ecs::Screen, ecs::TransformSnapshot, ecs::Name>(lock)) continue;
            if (gui.entity.Get<ecs::Gui>(lock).target != ecs::GuiTarget::World) continue;

            auto &screen = gui.entity.Get<ecs::Screen>(lock);
            gui.scale = screen.scale;

            graph.AddPass("Gui")
                .Build([&](rg::PassBuilder &builder) {
                    rg::ImageDesc desc;
                    desc.format = vk::Format::eR8G8B8A8Srgb;
                    desc.extent = vk::Extent3D(std::max(1, screen.resolution.x), std::max(1, screen.resolution.y), 1);
                    desc.mipLevels = CalculateMipmapLevels(desc.extent);
                    desc.sampler = SamplerType::TrilinearClampEdge;

                    auto name = "gui:" + gui.entity.Get<ecs::Name>(lock).String();
                    auto target = builder.OutputColorAttachment(0, name, desc, {LoadOp::Clear, StoreOp::Store});
                    gui.renderGraphID = target.id;
                })
                .Execute([this, gui](rg::Resources &resources, CommandContext &cmd) {
                    auto extent = resources.GetImageView(gui.renderGraphID)->Extent();
                    vk::Rect2D viewport = {{}, {extent.width, extent.height}};
                    guiRenderer->Render(*gui.context, cmd, viewport, gui.scale);
                });

            renderer::AddMipmap(graph, gui.renderGraphID);
        }
    }

    void Renderer::AddMenuGui(ecs::Lock<ecs::Read<ecs::View>> lock) {
        MenuGuiManager *menuManager = dynamic_cast<MenuGuiManager *>(menuGui);
        if (!menuManager || !menuManager->MenuOpen()) return;

        rg::ResourceID mipmapID = rg::InvalidResource;

        graph.AddPass("MenuGui")
            .Build([&](rg::PassBuilder &builder) {
                rg::ImageDesc desc;
                auto windowSize = CVarWindowSize.Get();
                desc.extent = vk::Extent3D(windowSize.x, windowSize.y, 1);
                desc.format = vk::Format::eR8G8B8A8Srgb;

                ecs::Entity windowEntity = device.GetActiveView();
                if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
                    auto view = windowEntity.Get<ecs::View>(lock);
                    desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);
                }
                desc.sampler = SamplerType::BilinearClampEdge;

                auto res = builder.OutputColorAttachment(0, "menu_gui", desc, {LoadOp::Clear, StoreOp::Store});

                if (desc.mipLevels > 1) mipmapID = res.id;
            })
            .Execute([this](rg::Resources &resources, CommandContext &cmd) {
                auto extent = resources.GetImageView("menu_gui")->Extent();
                vk::Rect2D viewport = {{}, {extent.width, extent.height}};
                auto windowScale = CVarWindowScale.Get();
                if (windowScale.x <= 0.0f) windowScale.x = 1.0f;
                if (windowScale.y <= 0.0f) windowScale.y = windowScale.x;

                guiRenderer->Render(*menuGui, cmd, viewport, windowScale);
            });

        if (mipmapID != rg::InvalidResource) renderer::AddMipmap(graph, mipmapID);
    }

    void Renderer::AddDeferredPasses(
        ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Screen, ecs::Gui, ecs::LaserLine>> lock,
        const ecs::View &view,
        chrono_clock::duration elapsedTime) {
        renderer::AddExposureState(graph);
        lighting.AddLightingPass(graph);
        renderer::AddSkyboxPass(graph);
        transparency.AddPass(graph, view);
        emissive.AddPass(graph, lock, elapsedTime);
        voxels.AddDebugPass(graph);
        renderer::AddExposureUpdate(graph);
        renderer::AddOutlines(graph, scene);
        renderer::AddBloom(graph);
        renderer::AddTonemap(graph);

        if (CVarSMAA.Get()) smaa.AddPass(graph);
    }

    void Renderer::AddMenuOverlay() {
        MenuGuiManager *menuManager = dynamic_cast<MenuGuiManager *>(menuGui);
        if (!menuManager || !menuManager->MenuOpen()) return;

        auto inputID = graph.LastOutputID();
        {
            auto scope = graph.Scope("MenuOverlayBlur");
            renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(0, 1), 2);
            renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(1, 0), 2);
            renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(0, 1), 1);
            renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(1, 0), 2);
            renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(0, 1), 1);
            renderer::AddGaussianBlur1D(graph, graph.LastOutputID(), glm::ivec2(1, 0), 1, 0.2f);
        }

        graph.AddPass("MenuOverlay")
            .Build([&](rg::PassBuilder &builder) {
                builder.Read(builder.LastOutputID(), Access::FragmentShaderSampleImage);
                builder.Read("menu_gui", Access::FragmentShaderSampleImage);

                auto desc = builder.GetResource(inputID).DeriveImage();
                builder.OutputColorAttachment(0, "Menu", desc, {LoadOp::DontCare, StoreOp::Store});
            })
            .Execute([](rg::Resources &resources, CommandContext &cmd) {
                cmd.DrawScreenCover(resources.GetImageView(resources.LastOutputID()));
                cmd.SetBlending(true);
                cmd.DrawScreenCover(resources.GetImageView("menu_gui"));
            });
    }

    void Renderer::EndFrame() {
        ZoneScoped;
        guiRenderer->Tick();

        auto setModel = [](auto &lock, ecs::Entity ent, AsyncPtr<Gltf> model) {
            if constexpr (Tecs::is_write_allowed<ecs::Renderable, std::decay_t<decltype(lock)>>()) {
                auto &renderable = ent.Get<ecs::Renderable>(lock);
                renderable.model = model;
                return true;
            } else {
                if (ecs::IsLive(ent)) {
                    ecs::QueueTransaction<ecs::Write<ecs::Renderable>>([ent, model](auto lock) {
                        if (!ent.Has<ecs::Renderable>(lock)) return;
                        auto &renderable = ent.Get<ecs::Renderable>(lock);
                        renderable.model = model;
                    });
                    return false;
                } else {
                    return true;
                }
            }
        };

        auto loadModel = [&](auto lock, ecs::Entity ent) {
            auto &renderable = ent.Get<ecs::Renderable>(lock);
            if (renderable.modelName.empty()) {
                setModel(lock, ent, nullptr);
                return true;
            } else if (!renderable.model) {
                if (!setModel(lock, ent, sp::Assets().LoadGltf(renderable.modelName))) {
                    return false;
                }
            }
            if (!renderable.model->Ready()) {
                return false;
            }

            auto model = renderable.model->Get();
            if (!model) {
                Errorf("Renderable %s model is null: %s", ecs::ToString(lock, ent), renderable.modelName);
                setModel(lock, ent, sp::Assets().LoadGltf(renderable.modelName));
                // Don't hang preloading if models are null
                return true;
            } else if (renderable.modelName != model->name) {
                setModel(lock, ent, sp::Assets().LoadGltf(renderable.modelName));
                return false;
            } else if (renderable.meshIndex >= model->meshes.size()) {
                Errorf("Renderable %s mesh index is out of range: %u/%u",
                    ecs::ToString(lock, ent),
                    renderable.meshIndex,
                    model->meshes.size());
                return true;
            }

            auto vkMesh = this->scene.LoadMesh(model, renderable.meshIndex);
            if (!vkMesh) {
                return false;
            }
            if (!vkMesh->CheckReady()) return false;
            return true;
        };

        GetSceneManager().PreloadSceneGraphics([&](auto lock, auto scene) {
            ZoneScopedN("PreloadSceneGraphics");
            bool complete = true;
            for (const ecs::Entity &ent : lock.template EntitiesWith<ecs::Renderable>()) {
                if (!ent.Has<ecs::SceneInfo>(lock)) continue;
                if (ent.Get<ecs::SceneInfo>(lock).scene != scene) continue;
                if (!loadModel(lock, ent)) complete = false;
            }
            if (!this->scene.PreloadTextures(lock)) complete = false;
            if (!smaa.PreloadTextures()) complete = false;
            return complete;
        });

        {
            auto lock = ecs::StartTransaction<ecs::Read<ecs::Name, ecs::Light, ecs::Renderable, ecs::Screen>>();
            bool anyTexturesChanged = false;
            ecs::ComponentModifiedEvent<ecs::Renderable> renderableEvent;
            while (renderableObserver.Poll(lock, renderableEvent)) {
                // std::string modelName = "";
                // if (renderableEvent.Has<ecs::Renderable>(lock)) {
                //     modelName = renderableEvent.Get<ecs::Renderable>(lock).modelName;
                // }
                // Logf("Renderable entity changed: %s %s", ecs::ToString(lock, renderableEvent), modelName);
                if (renderableEvent.Has<ecs::Renderable>(lock)) {
                    loadModel(lock, renderableEvent);
                }
                anyTexturesChanged = true;
            }

            ecs::ComponentModifiedEvent<ecs::Light> lightEvent;
            while (lightObserver.Poll(lock, lightEvent)) {
                // std::string filterName = "";
                // if (lightEvent.Has<ecs::Light>(lock)) {
                //     filterName = lightEvent.Get<ecs::Light>(lock).filterName;
                // }
                // Logf("Light entity changed: %s %s", ecs::ToString(lock, lightEvent), filterName);
                anyTexturesChanged = true;
            }
            if (anyTexturesChanged) scene.PreloadTextures(lock);
        }

        scene.Flush();
    }

    void Renderer::SetOverlayGui(GuiContext *gui) {
        overlayGui = gui;
    }

    void Renderer::SetMenuGui(GuiContext *gui) {
        menuGui = gui;
    }
} // namespace sp::vulkan
