#include "Renderer.hh"

#include "core/Logging.hh"
#include "core/Tracing.hh"
#include "ecs/EcsImpl.hh"
#include "game/SceneManager.hh"
#include "graphics/gui/MenuGuiManager.hh"
#include "graphics/gui/WorldGuiManager.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Image.hh"
#include "graphics/vulkan/core/Util.hh"
#include "graphics/vulkan/gui/GuiRenderer.hh"
#include "graphics/vulkan/render_passes/Bloom.hh"
#include "graphics/vulkan/render_passes/Blur.hh"
#include "graphics/vulkan/render_passes/Crosshair.hh"
#include "graphics/vulkan/render_passes/Exposure.hh"
#include "graphics/vulkan/render_passes/LightSensors.hh"
#include "graphics/vulkan/render_passes/Mipmap.hh"
#include "graphics/vulkan/render_passes/Outline.hh"
#include "graphics/vulkan/render_passes/Tonemap.hh"
#include "graphics/vulkan/render_passes/VisualizeBuffer.hh"
#include "graphics/vulkan/scene/Mesh.hh"
#include "graphics/vulkan/scene/VertexLayouts.hh"

#ifdef SP_XR_SUPPORT
    #include "xr/XrSystem.hh"
#endif

namespace sp::vulkan {
    static const std::string defaultWindowViewTarget = "FlatView.LastOutput";
    static const std::string defaultXRViewTarget = "XRView.LastOutput";

    CVar<string> CVarWindowViewTarget("r.WindowView", defaultWindowViewTarget, "Primary window's render target");

    static CVar<uint32> CVarWindowViewTargetLayer("r.WindowViewTargetLayer", 0, "Array layer to view");

    static CVar<string> CVarXRViewTarget("r.XRView", defaultXRViewTarget, "HMD's render target");

    static CVar<bool> CVarSMAA("r.SMAA", true, "Enable SMAA");

    Renderer::Renderer(DeviceContext &device)
        : device(device), graph(device), scene(device), lighting(scene), voxels(scene),
          guiRenderer(new GuiRenderer(device)) {
        funcs.Register("listgraphimages", "List all images in the render graph", [&]() {
            listImages = true;
        });

        auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
        guiObserver = lock.Watch<ecs::ComponentEvent<ecs::Gui>>();

        depthStencilFormat = device.SelectSupportedFormat(vk::FormatFeatureFlagBits::eDepthStencilAttachment,
            {vk::Format::eD24UnormS8Uint, vk::Format::eD32SfloatS8Uint, vk::Format::eD16UnormS8Uint});
    }

    Renderer::~Renderer() {
        device->waitIdle();
    }

    void Renderer::RenderFrame(chrono_clock::duration elapsedTime) {
        for (auto &gui : guis) {
            if (gui.contextShared) gui.contextShared->BeforeFrame();
        }

        BuildFrameGraph(elapsedTime);

        CVarWindowViewTarget.UpdateCompletions([&](vector<string> &completions) {
            auto list = graph.AllImages();
            for (const auto &info : list) {
                completions.push_back(info.name);
            }
        });

        CVarXRViewTarget.UpdateCompletions([&](vector<string> &completions) {
            auto list = graph.AllImages();
            for (const auto &info : list) {
                completions.push_back(info.name);
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
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name,
            ecs::TransformSnapshot,
            ecs::LaserLine,
            ecs::Light,
            ecs::LightSensor,
            ecs::VoxelArea,
            ecs::Renderable,
            ecs::View,
            ecs::XRView,
            ecs::OpticalElement,
            ecs::Gui,
            ecs::Screen,
            ecs::FocusLock>>();

        scene.LoadState(graph, lock);
        lighting.LoadState(graph, lock);
        voxels.LoadState(graph, lock);

        scene.AddGeometryWarp(graph);
        lighting.AddShadowPasses(graph);
        AddGuis(lock);
        lighting.AddGelTextures(graph);
        voxels.AddVoxelization(graph, lighting);
        renderer::AddLightSensors(graph, scene, lock);

#ifdef SP_XR_SUPPORT
        {
            auto scope = graph.Scope("XRView");
            AddXRView(lock);
            if (graph.HasResource("GBuffer0")) AddDeferredPasses(lock, elapsedTime);
        }
        AddXRSubmit(lock);
#endif

        {
            auto scope = graph.Scope("FlatView");
            AddFlatView(lock);
            if (graph.HasResource("GBuffer0")) {
                AddDeferredPasses(lock, elapsedTime);
                renderer::AddCrosshair(graph);
                if (lock.Get<ecs::FocusLock>().HasFocus(ecs::FocusLayer::MENU)) AddMenuOverlay();
            }
        }
        AddWindowOutput();
        screenshots.AddPass(graph);

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
                    cmd.SetImageView(0, 0, source);
                    cmd.DrawScreenCover(source);
                }

                if (debugGui) guiRenderer->Render(*debugGui, cmd, vk::Rect2D{{0, 0}, cmd.GetFramebufferExtent()});
            });

        graph.SetTargetImageView("WindowFinalOutput", swapchainImage);
    }

    void Renderer::AddFlatView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View>> lock) {
        ecs::Entity windowEntity = device.GetActiveView();

        if (!windowEntity || !windowEntity.Has<ecs::View>(lock)) return;

        auto view = windowEntity.Get<ecs::View>(lock);
        if (view.extents.x == 0 || view.extents.y == 0) {
            // TODO: Fix race condition with PrepareWindowView being in a separate transaction
            // Abortf("Invalid flatview extents: %d, %d", view.extents.x, view.extents.y);
            return;
        }
        view.UpdateViewMatrix(lock, windowEntity);

        auto drawIDs = scene.GenerateDrawsForView(graph, view.visibilityMask);

        graph.AddPass("ForwardPass")
            .Build([&](rg::PassBuilder &builder) {
                rg::ImageDesc desc;
                desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);
                desc.primaryViewType = vk::ImageViewType::e2DArray;

                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "GBuffer0", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR16G16Sfloat;
                builder.OutputColorAttachment(1, "GBuffer1", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR8Unorm;
                builder.OutputColorAttachment(2, "GBuffer2", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = depthStencilFormat;
                builder.OutputDepthAttachment("GBufferDepthStencil", desc, {LoadOp::Clear, StoreOp::Store});

                builder.CreateUniform("ViewState", sizeof(GPUViewState) * 2);

                builder.Read("WarpedVertexBuffer", Access::VertexBuffer);
                builder.Read(drawIDs.drawCommandsBuffer, Access::IndirectBuffer);
                builder.Read(drawIDs.drawParamsBuffer, Access::VertexShaderReadStorage);
            })
            .Execute([this, view, drawIDs](rg::Resources &resources, CommandContext &cmd) {
                cmd.SetShaders("scene.vert", "generate_gbuffer.frag");

                GPUViewState viewState[] = {{view}, {}};
                auto viewStateBuf = resources.GetBuffer("ViewState");
                viewStateBuf->CopyFrom(viewState, 2);
                cmd.SetUniformBuffer(0, 10, viewStateBuf);

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawIDs.drawCommandsBuffer),
                    resources.GetBuffer(drawIDs.drawParamsBuffer));
            });
    }

#ifdef SP_XR_SUPPORT
    void Renderer::AddXRView(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::View, ecs::XRView>> lock) {
        if (!xrSystem) return;

        auto xrViews = lock.EntitiesWith<ecs::XRView>();
        if (xrViews.size() == 0) return;

        glm::ivec2 viewExtents;
        std::array<ecs::View, (size_t)ecs::XrEye::Count> viewsByEye;

        for (auto &ent : xrViews) {
            if (!ent.Has<ecs::View>(lock)) continue;
            auto &view = ent.Get<ecs::View>(lock);

            if (viewExtents == glm::ivec2(0)) viewExtents = view.extents;
            Assert(viewExtents == view.extents, "All XR views must have the same extents");

            auto &xrView = ent.Get<ecs::XRView>(lock);
            viewsByEye[(size_t)xrView.eye] = view;
            viewsByEye[(size_t)xrView.eye].UpdateViewMatrix(lock, ent);
        }

        xrRenderPoses.resize(xrViews.size());

        if (!hiddenAreaMesh[0]) {
            for (size_t i = 0; i < hiddenAreaMesh.size(); i++) {
                auto mesh = xrSystem->GetHiddenAreaMesh(ecs::XrEye(i));
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

        auto drawIDs = scene.GenerateDrawsForView(graph, viewsByEye[0].visibilityMask);

        graph.AddPass("ForwardPass")
            .Build([&](rg::PassBuilder &builder) {
                rg::ImageDesc desc;
                desc.extent = vk::Extent3D(viewExtents.x, viewExtents.y, 1);
                desc.arrayLayers = xrViews.size();
                desc.primaryViewType = vk::ImageViewType::e2DArray;

                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "GBuffer0", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR16G16Sfloat;
                builder.OutputColorAttachment(1, "GBuffer1", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eR8Unorm;
                builder.OutputColorAttachment(2, "GBuffer2", desc, {LoadOp::Clear, StoreOp::Store});

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::Store});

                builder.CreateUniform("ViewState", sizeof(GPUViewState) * 2);

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
                cmd.SetUniformBuffer(0, 10, viewStateBuf);

                scene.DrawSceneIndirect(cmd,
                    resources.GetBuffer("WarpedVertexBuffer"),
                    resources.GetBuffer(drawIDs.drawCommandsBuffer),
                    resources.GetBuffer(drawIDs.drawParamsBuffer));

                GPUViewState *viewState;
                viewStateBuf->Map((void **)&viewState);
                for (size_t i = 0; i < viewsByEye.size(); i++) {
                    auto view = viewsByEye[i];

                    if (this->xrSystem->GetPredictedViewPose(ecs::XrEye(i), this->xrRenderPoses[i])) {
                        view.SetInvViewMat(view.invViewMat * this->xrRenderPoses[i]);
                    }

                    viewState[i] = GPUViewState(view);
                }
                viewStateBuf->Unmap();
                viewStateBuf->Flush();
            });
    }

    void Renderer::AddXRSubmit(ecs::Lock<ecs::Read<ecs::XRView>> lock) {
        if (!xrSystem) return;

        auto xrViews = lock.EntitiesWith<ecs::XRView>();
        if (xrViews.size() != 2) return;

        rg::ResourceID sourceID;
        graph.AddPass("XRSubmit")
            .Build([&](rg::PassBuilder &builder) {
                auto &sourceName = CVarXRViewTarget.Get();
                sourceID = builder.GetID(sourceName, false);
                if (sourceID == rg::InvalidResource && sourceName != defaultXRViewTarget) {
                    Errorf("image %s does not exist, defaulting to %s", sourceName, defaultXRViewTarget);
                    CVarXRViewTarget.Set(defaultXRViewTarget);
                    sourceID = builder.GetID(defaultXRViewTarget, false);
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
                auto xrImage = resources.GetImageView(sourceID);

                for (size_t i = 0; i < 2; i++) {
                    this->xrSystem->SubmitView(ecs::XrEye(i), this->xrRenderPoses[i], xrImage.get());
                }
            });
    }
#endif

    void Renderer::AddGuis(ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Gui, ecs::Screen>> lock) {
        ecs::ComponentEvent<ecs::Gui> guiEvent;
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

                const auto &guiComponent = eventEntity.Get<ecs::Gui>(lock);
                auto existingContext = std::get_if<GuiContext *>(&guiComponent.target);
                if (existingContext) {
                    guis.emplace_back(RenderableGui{eventEntity, *existingContext});
                } else {
                    auto &windowName = std::get<std::string>(guiComponent.target);
                    auto context = make_shared<WorldGuiManager>(eventEntity, windowName);
                    if (CreateGuiWindow(context.get(), windowName)) {
                        guis.emplace_back(RenderableGui{eventEntity, context.get(), context});
                    }
                }
            }
        }

        for (auto &gui : guis) {
            graph.AddPass("Gui")
                .Build([&](rg::PassBuilder &builder) {
                    rg::ImageDesc desc;
                    desc.format = vk::Format::eR8G8B8A8Srgb;
                    desc.extent = vk::Extent3D(1024, 1024, 1);

                    if (gui.entity.Has<ecs::Screen>(lock) && gui.entity.Has<ecs::TransformSnapshot>(lock)) {
                        auto tf = gui.entity.Get<ecs::TransformSnapshot>(lock);
                        desc.extent.height *= tf.GetScale().y / tf.GetScale().x;
                    }

                    MenuGuiManager *manager = dynamic_cast<MenuGuiManager *>(gui.context);
                    if (manager && manager->RenderMode() == MenuRenderMode::Pause) {
                        desc.extent = vk::Extent3D(1920, 1080, 1);
                    }

                    desc.mipLevels = CalculateMipmapLevels(desc.extent);
                    desc.sampler = SamplerType::TrilinearClampEdge;

                    auto name = gui.context->Name() + "_gui";
                    auto target = builder.OutputColorAttachment(0, name, desc, {LoadOp::Clear, StoreOp::Store});
                    gui.renderGraphID = target.id;
                })
                .Execute([this, gui](rg::Resources &resources, CommandContext &cmd) {
                    auto extent = resources.GetImageView(gui.renderGraphID)->Extent();
                    vk::Rect2D viewport = {{}, {extent.width, extent.height}};
                    guiRenderer->Render(*gui.context, cmd, viewport);
                });

            renderer::AddMipmap(graph, gui.renderGraphID);
        }
    }

    void Renderer::AddDeferredPasses(
        ecs::Lock<ecs::Read<ecs::TransformSnapshot, ecs::Screen, ecs::Gui, ecs::LaserLine>> lock,
        chrono_clock::duration elapsedTime) {
        renderer::AddExposureState(graph);
        lighting.AddLightingPass(graph);
        emissive.AddPass(graph, lock, elapsedTime);
        voxels.AddDebugPass(graph);
        renderer::AddExposureUpdate(graph);
        renderer::AddOutlines(graph, scene);
        renderer::AddBloom(graph);
        renderer::AddTonemap(graph);

        if (CVarSMAA.Get()) smaa.AddPass(graph);
    }

    void Renderer::AddMenuOverlay() {
        auto inputID = graph.LastOutputID();
        rg::ResourceID menuID = rg::InvalidResource;
        for (auto &gui : guis) {
            if (gui.context->Name() == "menu") {
                menuID = gui.renderGraphID;
                MenuGuiManager *manager = dynamic_cast<MenuGuiManager *>(gui.context);
                if (!manager || manager->RenderMode() != MenuRenderMode::Pause) return;
                break;
            }
        }
        Assert(menuID != rg::InvalidResource, "main menu doesn't exist");

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
                builder.Read(menuID, Access::FragmentShaderSampleImage);

                auto desc = builder.GetResource(inputID).DeriveImage();
                builder.OutputColorAttachment(0, "Menu", desc, {LoadOp::DontCare, StoreOp::Store});
            })
            .Execute([menuID](rg::Resources &resources, CommandContext &cmd) {
                cmd.DrawScreenCover(resources.GetImageView(resources.LastOutputID()));
                cmd.SetBlending(true);
                cmd.DrawScreenCover(resources.GetImageView(menuID));
            });
    }

    void Renderer::EndFrame() {
        ZoneScoped;
        guiRenderer->Tick();

        GetSceneManager().PreloadSceneGraphics([this](auto lock, auto scene) {
            bool complete = true;
            for (auto ent : lock.template EntitiesWith<ecs::Renderable>()) {
                if (!ent.template Has<ecs::SceneInfo>(lock)) continue;
                if (ent.template Get<ecs::SceneInfo>(lock).scene.lock() != scene) continue;

                auto &renderable = ent.template Get<ecs::Renderable>(lock);
                if (!renderable.model) continue;
                if (!renderable.model->Ready()) {
                    complete = false;
                    continue;
                }

                auto model = renderable.model->Get();
                if (!model) {
                    Errorf("Preloading renderable with null model: %s", ecs::ToString(lock, ent));
                    continue;
                }

                auto vkMesh = this->scene.LoadMesh(model, renderable.meshIndex);
                if (!vkMesh) {
                    complete = false;
                    continue;
                }
                if (!vkMesh->CheckReady()) complete = false;
            }
            return complete;
        });

        scene.Flush();
    }

    void Renderer::SetDebugGui(GuiContext &gui) {
        debugGui = &gui;
    }
} // namespace sp::vulkan
