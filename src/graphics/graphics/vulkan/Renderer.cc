#include "Renderer.hh"

#include "assets/Model.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/gui/MenuGuiManager.hh"
#include "graphics/vulkan/GuiRenderer.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Image.hh"
#include "graphics/vulkan/core/Model.hh"
#include "graphics/vulkan/core/RenderGraph.hh"
#include "graphics/vulkan/core/Screenshot.hh"
#include "graphics/vulkan/core/Util.hh"
#include "graphics/vulkan/core/Vertex.hh"

#ifdef SP_XR_SUPPORT
    #include "xr/XrSystem.hh"
#endif

namespace sp::vulkan {
    static const char *defaultWindowViewTarget = "FlatView.LastOutput";
    static const char *defaultXRViewTarget = "XRView.LastOutput";

    static CVar<string> CVarWindowViewTarget("r.WindowViewTarget",
        defaultWindowViewTarget,
        "Primary window's render target");

    static CVar<uint32> CVarWindowViewTargetLayer("r.WindowViewTargetLayer", 0, "Array layer to view");

    static CVar<string> CVarXRViewTarget("r.XRViewTarget", defaultXRViewTarget, "HMD's render target");

    static CVar<float> CVarBloomScale("r.BloomScale", 0.15f, "Bloom scale");

    Renderer::Renderer(DeviceContext &device, PerfTimer &timer) : device(device), timer(timer) {
        funcs.Register<string, string>("screenshot",
            "Save screenshot to <path>, optionally specifying an image <resource>",
            [&](string path, string resource) {
                QueueScreenshot(path, resource);
            });

        funcs.Register("listrendertargets", "List all render targets", [&]() {
            listRenderTargets = true;
        });

        auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
        guiObserver = lock.Watch<ecs::ComponentEvent<ecs::Gui>>();
    }

    Renderer::~Renderer() {
        device->waitIdle();
    }

    void Renderer::AddLightState(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::Light, ecs::Transform>> lock) {
        int lightCount = 0;
        int gelCount = 0;
        glm::ivec2 renderTargetSize(0, 0);

        for (auto entity : lock.EntitiesWith<ecs::Light>()) {
            if (!entity.Has<ecs::Light, ecs::Transform>(lock)) continue;

            auto &light = entity.Get<ecs::Light>(lock);
            if (!light.on) continue;

            int extent = (int)std::pow(2, light.shadowMapSize);

            auto &transform = entity.Get<ecs::Transform>(lock);
            auto &view = lights.views[lightCount];

            view.visibilityMask.set(ecs::Renderable::VISIBLE_LIGHTING_SHADOW);
            view.extents = {extent, extent};
            view.fov = light.spotAngle * 2.0f;
            view.offset = {renderTargetSize.x, 0};
            view.clearMode.reset();
            view.clip = light.shadowMapClip;
            view.UpdateProjectionMatrix();
            view.UpdateViewMatrix(lock, entity);

            auto &data = lights.gpuData.lights[lightCount];
            data.position = transform.GetGlobalPosition(lock);
            data.tint = light.tint;
            data.direction = transform.GetGlobalForward(lock);
            data.spotAngleCos = cos(light.spotAngle);
            data.proj = view.projMat;
            data.invProj = view.invProjMat;
            data.view = view.viewMat;
            data.clip = view.clip;
            data.mapOffset = {renderTargetSize.x, 0, extent, extent};
            data.intensity = light.intensity;
            data.illuminance = light.illuminance;

            if (light.gelName.empty()) {
                data.gelId = 0;
            } else {
                Assert(gelCount < MAX_LIGHT_GELS, "too many light gels");
                data.gelId = ++gelCount;
                lights.gelNames[data.gelId - 1] = light.gelName;
            }

            renderTargetSize.x += extent;
            if (extent > renderTargetSize.y) renderTargetSize.y = extent;
            if (++lightCount >= MAX_LIGHTS) break;
        }
        glm::vec4 mapOffsetScale(renderTargetSize, renderTargetSize);
        for (int i = 0; i < lightCount; i++) {
            lights.gpuData.lights[i].mapOffset /= mapOffsetScale;
        }
        lights.renderTargetSize = renderTargetSize;
        lights.count = lightCount;
        lights.gelCount = gelCount;
        lights.gpuData.count = lightCount;

        graph.Pass("LightState")
            .Build([&](RenderGraphPassBuilder &builder) {
                builder.CreateUniformBuffer("LightState", sizeof(lights.gpuData));
            })
            .Execute([this](RenderGraphResources &resources, DeviceContext &) {
                resources.GetBuffer("LightState")->CopyFrom(&lights.gpuData);
            });
    }

    void Renderer::RenderFrame() {
        RenderGraph graph(device, &timer);
        BuildFrameGraph(graph);
        graph.Execute();
        EndFrame();
    }

    void Renderer::BuildFrameGraph(RenderGraph &graph) {
        // lock is copied into the Execute closure of some passes,
        // and will be released when those passes are done executing.
        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Name,
            ecs::Transform,
            ecs::Light,
            ecs::Renderable,
            ecs::View,
            ecs::XRView,
            ecs::Mirror,
            ecs::Gui,
            ecs::FocusLock>>();

        AddLightState(graph, lock);
        AddShadowMaps(graph, lock);
        AddGuis(graph, lock);

        Tecs::Entity windowEntity = device.GetActiveView();

        if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
            auto view = windowEntity.Get<ecs::View>(lock);
            view.UpdateViewMatrix(lock, windowEntity);

            graph.BeginScope("FlatView");

            graph.Pass("ForwardPass")
                .Build([&](RenderGraphPassBuilder &builder) {
                    RenderTargetDesc desc;
                    desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);
                    desc.primaryViewType = vk::ImageViewType::e2DArray;

                    desc.format = vk::Format::eR8G8B8A8Srgb;
                    builder.OutputColorAttachment(0, "GBuffer0", desc, {LoadOp::Clear, StoreOp::Store});

                    desc.format = vk::Format::eR16G16B16A16Sfloat;
                    builder.OutputColorAttachment(1, "GBuffer1", desc, {LoadOp::Clear, StoreOp::Store});
                    builder.OutputColorAttachment(2, "GBuffer2", desc, {LoadOp::Clear, StoreOp::Store});

                    desc.format = vk::Format::eD24UnormS8Uint;
                    builder.OutputDepthAttachment("GBufferDepthStencil", desc, {LoadOp::Clear, StoreOp::Store});

                    builder.CreateUniformBuffer("ViewState", sizeof(GPUViewState) * 2);
                })
                .Execute([this, view, lock](RenderGraphResources &resources, CommandContext &cmd) {
                    cmd.SetShaders("scene.vert", "generate_gbuffer.frag");

                    GPUViewState viewState[] = {{view}, {}};
                    auto viewStateBuf = resources.GetBuffer("ViewState");
                    viewStateBuf->CopyFrom(viewState, 2);
                    cmd.SetUniformBuffer(0, 10, viewStateBuf);

                    this->ForwardPass(cmd, view.visibilityMask, lock);
                });

            AddDeferredPasses(graph);

            if (lock.Get<ecs::FocusLock>().HasFocus(ecs::FocusLayer::MENU)) AddMenuOverlay(graph);

            graph.EndScope();
        }

#ifdef SP_XR_SUPPORT
        auto xrViews = lock.EntitiesWith<ecs::XRView>();
        if (xrSystem && xrViews.size() > 0) {
            ecs::View firstView;
            for (auto &ent : xrViews) {
                if (ent.Has<ecs::View>(lock)) {
                    auto &view = ent.Get<ecs::View>(lock);

                    if (firstView.extents == glm::ivec2(0)) {
                        firstView = view;
                    } else if (firstView.extents != view.extents) {
                        Abort("All XR views must have the same extents");
                    }
                }
            }

            xrRenderPoses.resize(xrViews.size());
            auto visible = firstView.visibilityMask;

            graph.BeginScope("XRView");

            if (!hiddenAreaMesh[0]) {
                auto leftEye = xrSystem->GetHiddenAreaMesh(ecs::XrEye::Left);
                hiddenAreaMesh[0] = device.CreateBuffer(leftEye.vertices,
                    leftEye.triangleCount * sizeof(glm::vec2) * 3,
                    vk::BufferUsageFlagBits::eVertexBuffer,
                    VMA_MEMORY_USAGE_CPU_TO_GPU);
                hiddenAreaTriangleCount[0] = leftEye.triangleCount;

                auto rightEye = xrSystem->GetHiddenAreaMesh(ecs::XrEye::Right);
                hiddenAreaMesh[1] = device.CreateBuffer(rightEye.vertices,
                    rightEye.triangleCount * sizeof(glm::vec2) * 3,
                    vk::BufferUsageFlagBits::eVertexBuffer,
                    VMA_MEMORY_USAGE_CPU_TO_GPU);
                hiddenAreaTriangleCount[1] = rightEye.triangleCount;
            }

            auto executeHiddenAreaStencil = [this](uint32 eyeIndex) {
                return [this, eyeIndex](RenderGraphResources &resources, CommandContext &cmd) {
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

            graph.Pass("HiddenAreaStencil")
                .Build([&](RenderGraphPassBuilder &builder) {
                    RenderTargetDesc desc;
                    desc.extent = vk::Extent3D(firstView.extents.x, firstView.extents.y, 1);
                    desc.arrayLayers = xrViews.size();
                    desc.primaryViewType = vk::ImageViewType::e2DArray;
                    desc.format = vk::Format::eD24UnormS8Uint;
                    AttachmentInfo attachment = {LoadOp::Clear, StoreOp::Store};
                    attachment.arrayIndex = 0;
                    builder.OutputDepthAttachment("GBufferDepthStencil", desc, attachment);
                })
                .Execute(executeHiddenAreaStencil(0));

            graph.Pass("HiddenAreaStencil2")
                .Build([&](RenderGraphPassBuilder &builder) {
                    AttachmentInfo attachment = {LoadOp::Clear, StoreOp::Store};
                    attachment.arrayIndex = 1;
                    builder.SetDepthAttachment("GBufferDepthStencil", attachment);
                })
                .Execute(executeHiddenAreaStencil(1));

            graph.Pass("ForwardPass")
                .Build([&](RenderGraphPassBuilder &builder) {
                    RenderTargetDesc desc;
                    desc.extent = vk::Extent3D(firstView.extents.x, firstView.extents.y, 1);
                    desc.arrayLayers = xrViews.size();
                    desc.primaryViewType = vk::ImageViewType::e2DArray;

                    desc.format = vk::Format::eR8G8B8A8Srgb;
                    builder.OutputColorAttachment(0, "GBuffer0", desc, {LoadOp::Clear, StoreOp::Store});

                    desc.format = vk::Format::eR16G16B16A16Sfloat;
                    builder.OutputColorAttachment(1, "GBuffer1", desc, {LoadOp::Clear, StoreOp::Store});
                    builder.OutputColorAttachment(2, "GBuffer2", desc, {LoadOp::Clear, StoreOp::Store});

                    builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::DontCare});

                    builder.CreateUniformBuffer("ViewState", sizeof(GPUViewState) * 2);
                })
                .Execute([this, xrViews, visible, lock](RenderGraphResources &resources, CommandContext &cmd) {
                    cmd.SetShaders("scene.vert", "generate_gbuffer.frag");

                    cmd.SetStencilTest(true);
                    cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                    cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                    cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                    auto viewStateBuf = resources.GetBuffer("ViewState");
                    cmd.SetUniformBuffer(0, 10, viewStateBuf);

                    this->ForwardPass(cmd, visible, lock);

                    GPUViewState *viewState;
                    viewStateBuf->Map((void **)&viewState);
                    for (size_t i = 0; i < xrViews.size(); i++) {
                        if (!xrViews[i].Has<ecs::View, ecs::XRView>(lock)) continue;
                        auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;
                        auto view = xrViews[i].Get<ecs::View>(lock);
                        view.UpdateViewMatrix(lock, xrViews[i]);

                        if (this->xrSystem->GetPredictedViewPose(eye, this->xrRenderPoses[i])) {
                            view.SetViewMat(glm::inverse(this->xrRenderPoses[i]) * view.viewMat);
                        }

                        viewState[i] = GPUViewState(view);
                    }
                    viewStateBuf->Unmap();
                    viewStateBuf->Flush();
                });

            AddDeferredPasses(graph);
            graph.EndScope();

            RenderGraphResourceID sourceID;
            graph.Pass("XRSubmit")
                .Build([&](RenderGraphPassBuilder &builder) {
                    auto res = builder.GetResource(CVarXRViewTarget.Get());
                    if (!res) {
                        Errorf("view target %s does not exist, defaulting to %s",
                            CVarXRViewTarget.Get(),
                            defaultXRViewTarget);
                        CVarXRViewTarget.Set(defaultXRViewTarget);
                        res = builder.GetResource(defaultXRViewTarget);
                    }

                    auto format = res.renderTargetDesc.format;
                    if (FormatComponentCount(format) == 4 && FormatByteSize(format) == 4) {
                        sourceID = res.id;
                    } else {
                        sourceID = VisualizeBuffer(graph, res.id);
                    }

                    builder.TransferRead(sourceID);
                    builder.RequirePass();
                })
                .Execute([this, xrViews, sourceID, lock](RenderGraphResources &resources, DeviceContext &device) {
                    auto xrRenderTarget = resources.GetRenderTarget(sourceID);

                    for (size_t i = 0; i < xrViews.size(); i++) {
                        auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;
                        this->xrSystem->SubmitView(eye, this->xrRenderPoses[i], xrRenderTarget->ImageView().get());
                    }
                });
        }
#endif

        auto swapchainImage = device.SwapchainImageView();
        if (windowEntity && windowEntity.Has<ecs::View>(lock) && swapchainImage) {
            auto view = windowEntity.Get<ecs::View>(lock);

            RenderGraphResourceID sourceID;
            graph.Pass("WindowFinalOutput")
                .Build([&](RenderGraphPassBuilder &builder) {
                    auto res = builder.GetResource(CVarWindowViewTarget.Get());
                    if (!res) {
                        Errorf("view target %s does not exist, defaulting to %s",
                            CVarWindowViewTarget.Get(),
                            defaultWindowViewTarget);
                        CVarWindowViewTarget.Set(defaultWindowViewTarget);
                        res = builder.GetResource(defaultWindowViewTarget);
                    }

                    auto format = res.renderTargetDesc.format;
                    auto layer = CVarWindowViewTargetLayer.Get();
                    if (FormatComponentCount(format) == 4 && FormatByteSize(format) == 4 && layer == 0) {
                        sourceID = res.id;
                    } else {
                        sourceID = VisualizeBuffer(graph, res.id, layer);
                    }
                    builder.ShaderRead(sourceID);

                    RenderTargetDesc desc;
                    desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);
                    desc.format = swapchainImage->Format();
                    builder.OutputColorAttachment(0, "WindowFinalOutput", desc, {LoadOp::DontCare, StoreOp::Store});
                })
                .Execute([this, sourceID](RenderGraphResources &resources, CommandContext &cmd) {
                    auto source = resources.GetRenderTarget(sourceID)->ImageView();
                    cmd.SetTexture(0, 0, source);
                    cmd.DrawScreenCover(source);

                    if (this->debugGuiRenderer) {
                        this->debugGuiRenderer->Render(cmd, vk::Rect2D{{0, 0}, cmd.GetFramebufferExtent()});
                    }
                });

            graph.SetTargetImageView("WindowFinalOutput", swapchainImage);
            graph.RequireResource("WindowFinalOutput");
        }

        AddScreenshotPasses(graph);

        if (listRenderTargets) {
            listRenderTargets = false;
            auto list = graph.AllRenderTargets();
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
    }

    void Renderer::AddMenuOverlay(RenderGraph &graph) {
        RenderGraphResourceID menuID = RenderGraphResources::npos;
        for (auto &gui : guis) {
            if (gui.renderer->Name() == "menu_gui") {
                menuID = gui.renderGraphID;
                MenuGuiManager *manager = dynamic_cast<MenuGuiManager *>(gui.renderer->Manager());
                if (!manager || manager->RenderMode() != MenuRenderMode::Pause) return;
                break;
            }
        }
        Assert(menuID != RenderGraphResources::npos, "main menu doesn't exist");

        graph.Pass("MenuOverlay")
            .Build([&](RenderGraphPassBuilder &builder) {
                auto input = builder.LastOutput();
                builder.ShaderRead(input.id);

                auto desc = input.renderTargetDesc;
                desc.usage = {}; // TODO: usage will be leaked between targets unless it's reset like this
                builder.OutputColorAttachment(0, "Menu", desc, {LoadOp::DontCare, StoreOp::Store});

                builder.ShaderRead(menuID);
            })
            .Execute([menuID](RenderGraphResources &resources, CommandContext &cmd) {
                cmd.DrawScreenCover(resources.GetRenderTarget(resources.LastOutputID())->ImageView());
                cmd.DrawScreenCover(resources.GetRenderTarget(menuID)->ImageView());
            });
    }

    void Renderer::AddShadowMaps(RenderGraph &graph, DrawLock lock) {
        graph.Pass("ShadowMaps")
            .Build([&](RenderGraphPassBuilder &builder) {
                RenderTargetDesc desc;
                auto extent = glm::max(glm::ivec2(1), lights.renderTargetSize);
                desc.extent = vk::Extent3D(extent.x, extent.y, 1);
                desc.format = vk::Format::eR32Sfloat;
                builder.OutputColorAttachment(0, "ShadowMapLinear", desc, {LoadOp::Clear, StoreOp::Store});

                desc.format = vk::Format::eD16Unorm;
                builder.OutputDepthAttachment("ShadowMapDepth", desc, {LoadOp::Clear, StoreOp::Store});
            })
            .Execute([this, lock](RenderGraphResources &resources, CommandContext &cmd) {
                cmd.SetShaders("shadow_map.vert", "shadow_map.frag");

                auto &lights = this->lights;
                for (int i = 0; i < lights.count; i++) {
                    auto &view = lights.views[i];

                    GPUViewState views[] = {{view}, {}};
                    cmd.UploadUniformData(0, 10, views, 2);

                    vk::Rect2D viewport;
                    viewport.extent = vk::Extent2D(view.extents.x, view.extents.y);
                    viewport.offset = vk::Offset2D(view.offset.x, view.offset.y);
                    cmd.SetViewport(viewport);
                    cmd.SetYDirection(YDirection::Down);

                    this->ForwardPass(cmd, view.visibilityMask, lock, false);
                }
            });
    }

    void Renderer::AddScreenshotPasses(RenderGraph &graph) {
        for (auto &pending : pendingScreenshots) {
            auto screenshotPath = pending.first;
            auto screenshotResource = pending.second;
            if (screenshotResource.empty()) screenshotResource = CVarWindowViewTarget.Get();

            RenderGraphResourceID sourceID = ~0u;

            graph.Pass("Screenshot")
                .Build([&](RenderGraphPassBuilder &builder) {
                    auto resource = builder.GetResource(screenshotResource);
                    if (resource.type != RenderGraphResource::Type::RenderTarget) {
                        Errorf("Can't screenshot \"%s\": invalid resource", screenshotResource);
                    } else {
                        auto format = resource.renderTargetDesc.format;
                        if (FormatByteSize(format) == FormatComponentCount(format)) {
                            sourceID = resource.id;
                        } else {
                            sourceID = VisualizeBuffer(graph, resource.id);
                        }
                        builder.TransferRead(sourceID);
                        builder.RequirePass();
                    }
                })
                .Execute([screenshotPath, sourceID](RenderGraphResources &resources, DeviceContext &device) {
                    auto &res = resources.GetResource(sourceID);
                    if (res.type == RenderGraphResource::Type::RenderTarget) {
                        auto target = resources.GetRenderTarget(res.id);
                        WriteScreenshot(device, screenshotPath, target->ImageView());
                    }
                });
        }
        pendingScreenshots.clear();
    }

    RenderGraphResourceID Renderer::VisualizeBuffer(RenderGraph &graph,
        RenderGraphResourceID sourceID,
        uint32 arrayLayer) {

        RenderGraphResourceID targetID = ~0u, outputID;
        graph.Pass("VisualizeBuffer")
            .Build([&](RenderGraphPassBuilder &builder) {
                auto &res = builder.ShaderRead(sourceID);
                targetID = res.id;
                auto desc = res.renderTargetDesc;
                desc.format = vk::Format::eR8G8B8A8Srgb;
                desc.usage = {}; // TODO: usage will be leaked between targets unless it's reset like this
                outputID = builder.OutputColorAttachment(0, "", desc, {LoadOp::DontCare, StoreOp::Store}).id;
            })
            .Execute([this, targetID, arrayLayer](RenderGraphResources &resources, CommandContext &cmd) {
                auto target = resources.GetRenderTarget(targetID);
                ImageViewPtr source;
                if (target->Desc().arrayLayers > 1 && arrayLayer != ~0u && arrayLayer < target->Desc().arrayLayers) {
                    source = target->LayerImageView(arrayLayer);
                } else {
                    source = target->ImageView();
                }

                auto debugView = this->debugViews.Load(source.get());
                if (debugView) {
                    source = debugView;
                } else if (source->Format() == vk::Format::eD24UnormS8Uint) {
                    auto viewInfo = source->CreateInfo();
                    viewInfo.aspectMask = vk::ImageAspectFlagBits::eStencil;
                    viewInfo.defaultSampler = cmd.Device().GetSampler(SamplerType::BilinearClamp);
                    debugView = cmd.Device().CreateImageView(viewInfo);
                    this->debugViews.Register(source.get(), debugView);
                    source = debugView;
                }

                if (source->ViewType() == vk::ImageViewType::e2DArray) {
                    cmd.SetShaders("screen_cover.vert", "visualize_buffer_2d_array.frag");

                    struct {
                        float layer = 0;
                    } push;
                    cmd.PushConstants(push);
                } else {
                    cmd.SetShaders("screen_cover.vert", "visualize_buffer_2d.frag");
                }

                auto format = source->Format();
                uint32 comp = FormatComponentCount(format);
                uint32 swizzle = 0b11000000; // rrra
                if (comp > 1) {
                    swizzle = 0b11100100; // rgba
                }
                cmd.SetShaderConstant(ShaderStage::Fragment, 0, swizzle);

                cmd.SetTexture(0, 0, source);
                cmd.Draw(3);
            });
        return outputID;
    }

    void Renderer::AddGuis(RenderGraph &graph, ecs::Lock<ecs::Read<ecs::Gui>> lock) {
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
                const auto &guiComponent = eventEntity.Get<ecs::Gui>(lock);
                guis.emplace_back(
                    RenderableGui{guiEvent.entity, make_shared<GuiRenderer>(device, *guiComponent.manager)});
            }
        }

        for (auto &gui : guis) {
            graph.Pass("Gui")
                .Build([&](RenderGraphPassBuilder &builder) {
                    RenderTargetDesc desc;
                    desc.format = vk::Format::eR8G8B8A8Srgb;

                    // TODO: figure out a good size based on the transform of the gui
                    desc.extent = vk::Extent3D(1024, 1024, 1);
                    MenuGuiManager *manager = dynamic_cast<MenuGuiManager *>(gui.renderer->Manager());
                    if (manager && manager->RenderMode() == MenuRenderMode::Pause) {
                        desc.extent = vk::Extent3D(1920, 1080, 1);
                    }

                    const auto &name = gui.renderer->Name();
                    auto target = builder.OutputColorAttachment(0, name, desc, {LoadOp::Clear, StoreOp::Store});
                    gui.renderGraphID = target.id;
                })
                .Execute([gui](RenderGraphResources &resources, CommandContext &cmd) {
                    auto &extent = resources.GetRenderTarget(gui.renderGraphID)->Desc().extent;
                    vk::Rect2D viewport = {{}, {extent.width, extent.height}};
                    gui.renderer->Render(cmd, viewport);
                });
        }
    }

    void Renderer::AddDeferredPasses(RenderGraph &graph) {
        RenderGraphResourceID luminance;
        graph.Pass("Lighting")
            .Build([&](RenderGraphPassBuilder &builder) {
                auto gBuffer0 = builder.ShaderRead("GBuffer0");
                builder.ShaderRead("GBuffer1");
                builder.ShaderRead("GBuffer2");
                builder.ShaderRead("ShadowMapLinear");

                auto desc = gBuffer0.renderTargetDesc;
                desc.usage = {}; // TODO: usage will be leaked between targets unless it's reset like this
                desc.format = vk::Format::eR16G16B16A16Sfloat;
                auto dest =
                    builder.OutputColorAttachment(0, "LinearLuminance", desc, {LoadOp::DontCare, StoreOp::Store});
                luminance = dest.id;

                builder.ReadBuffer("ViewState");
                builder.ReadBuffer("LightState");

                for (int i = 0; i < this->lights.gelCount; i++) {
                    builder.ShaderRead(this->lights.gelNames[i]);
                }

                builder.SetDepthAttachment("GBufferDepthStencil", {LoadOp::Load, StoreOp::DontCare});
            })
            .Execute([this](RenderGraphResources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "lighting.frag");
                cmd.SetStencilTest(true);
                cmd.SetDepthTest(false, false);
                cmd.SetStencilCompareOp(vk::CompareOp::eNotEqual);
                cmd.SetStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 1);
                cmd.SetStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, 1);

                cmd.SetTexture(0, 0, resources.GetRenderTarget("GBuffer0")->ImageView());
                cmd.SetTexture(0, 1, resources.GetRenderTarget("GBuffer1")->ImageView());
                cmd.SetTexture(0, 2, resources.GetRenderTarget("GBuffer2")->ImageView());
                cmd.SetTexture(0, 3, resources.GetRenderTarget("ShadowMapLinear")->ImageView());

                for (int i = 0; i < MAX_LIGHT_GELS; i++) {
                    if (i < this->lights.gelCount) {
                        const auto &target = resources.GetRenderTarget(this->lights.gelNames[i]);
                        cmd.SetTexture(1, i, target->ImageView());
                    } else {
                        cmd.SetTexture(1, i, this->GetBlankPixelImage());
                    }
                }

                cmd.SetUniformBuffer(0, 10, resources.GetBuffer("ViewState"));
                cmd.SetUniformBuffer(0, 11, resources.GetBuffer("LightState"));
                cmd.Draw(3);
            });

        auto bloom = AddBloom(graph, luminance);

        graph.Pass("Tonemap")
            .Build([&](RenderGraphPassBuilder &builder) {
                auto luminance = builder.ShaderRead(bloom);

                auto desc = luminance.renderTargetDesc;
                desc.usage = {}; // TODO: usage will be leaked between targets unless it's reset like this
                desc.format = vk::Format::eR8G8B8A8Srgb;
                builder.OutputColorAttachment(0, "TonemappedLuminance", desc, {LoadOp::DontCare, StoreOp::Store});
            })
            .Execute([bloom](RenderGraphResources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "tonemap.frag");
                cmd.SetTexture(0, 0, resources.GetRenderTarget(bloom)->ImageView());
                cmd.Draw(3);
            });
    }

    RenderGraphResourceID Renderer::AddGaussianBlur(RenderGraph &graph,
        RenderGraphResourceID sourceID,
        glm::ivec2 direction,
        uint32 downsample,
        float scale,
        float clip) {

        struct {
            glm::vec2 direction;
            float threshold;
            float scale;
        } constants;

        constants.direction = glm::vec2(direction);
        constants.threshold = clip;
        constants.scale = scale;

        RenderGraphResourceID destID;
        graph.Pass("GaussianBlur")
            .Build([&](RenderGraphPassBuilder &builder) {
                auto source = builder.ShaderRead(sourceID);
                auto desc = source.renderTargetDesc;
                desc.extent.width /= downsample;
                desc.extent.height /= downsample;
                desc.usage = {}; // TODO: usage will be leaked between targets unless it's reset like this
                auto dest = builder.OutputColorAttachment(0, "", desc, {LoadOp::DontCare, StoreOp::Store});
                destID = dest.id;
            })
            .Execute([sourceID, constants](RenderGraphResources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "gaussian_blur.frag");
                cmd.SetTexture(0, 0, resources.GetRenderTarget(sourceID)->ImageView());
                cmd.PushConstants(constants);
                cmd.Draw(3);
            });
        return destID;
    }

    RenderGraphResourceID Renderer::AddBloom(RenderGraph &graph, RenderGraphResourceID sourceID) {
        auto blurY1 = AddGaussianBlur(graph, sourceID, glm::ivec2(0, 1), 1, CVarBloomScale.Get());
        auto blurX1 = AddGaussianBlur(graph, blurY1, glm::ivec2(1, 0), 2);
        auto blurY2 = AddGaussianBlur(graph, blurX1, glm::ivec2(0, 1), 1);
        auto blurX2 = AddGaussianBlur(graph, blurY2, glm::ivec2(1, 0), 1);

        RenderGraphResourceID destID;
        graph.Pass("BloomCombine")
            .Build([&](RenderGraphPassBuilder &builder) {
                auto source = builder.ShaderRead(sourceID);
                auto desc = source.renderTargetDesc;
                desc.usage = {}; // TODO: usage will be leaked between targets unless it's reset like this
                auto dest = builder.OutputColorAttachment(0, "Bloom", desc, {LoadOp::DontCare, StoreOp::Store});
                destID = dest.id;

                builder.ShaderRead(blurX1);
                builder.ShaderRead(blurX2);
            })
            .Execute([sourceID, blurX1, blurX2](RenderGraphResources &resources, CommandContext &cmd) {
                cmd.SetShaders("screen_cover.vert", "bloom_combine.frag");
                cmd.SetTexture(0, 0, resources.GetRenderTarget(sourceID)->ImageView());
                cmd.SetTexture(0, 1, resources.GetRenderTarget(blurX1)->ImageView());
                cmd.SetTexture(0, 2, resources.GetRenderTarget(blurX2)->ImageView());
                cmd.Draw(3);
            });
        return destID;
    }

    void Renderer::ForwardPass(CommandContext &cmd,
        ecs::Renderable::VisibilityMask viewMask,
        DrawLock lock,
        bool useMaterial,
        const PreDrawFunc &preDraw) {
        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform>(lock)) {
                if (ent.Has<ecs::Mirror>(lock)) continue;
                DrawEntity(cmd, viewMask, lock, ent, useMaterial, preDraw);
            }
        }

        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform, ecs::Mirror>(lock)) {
                DrawEntity(cmd, viewMask, lock, ent, useMaterial, preDraw);
            }
        }
    }

    void Renderer::DrawEntity(CommandContext &cmd,
        ecs::Renderable::VisibilityMask viewMask,
        DrawLock lock,
        Tecs::Entity &ent,
        bool useMaterial,
        const PreDrawFunc &preDraw) {
        auto &comp = ent.Get<ecs::Renderable>(lock);
        if (!comp.model || !comp.model->Valid()) return;

        // Filter entities that aren't members of all layers in the view's visibility mask.
        ecs::Renderable::VisibilityMask mask = comp.visibility;
        mask &= viewMask;
        if (mask != viewMask) return;

        glm::mat4 modelMat = ent.Get<ecs::Transform>(lock).GetGlobalTransform(lock);

        if (preDraw) preDraw(lock, ent);

        auto model = activeModels.Load(comp.model->name);
        if (!model) {
            model = make_shared<Model>(*comp.model, device);
            activeModels.Register(comp.model->name, model);
        }

        model->Draw(cmd, modelMat, useMaterial); // TODO pass and use comp.model->bones
    }

    void Renderer::EndFrame() {
        activeModels.Tick(std::chrono::milliseconds(11)); // TODO: look up the target framerate
    }

    void Renderer::SetDebugGui(GuiManager &gui) {
        debugGuiRenderer = make_unique<GuiRenderer>(device, gui);
    }

    void Renderer::QueueScreenshot(const string &path, const string &resource) {
        pendingScreenshots.push_back({path, resource});
    }

    ImageViewPtr Renderer::GetBlankPixelImage() {
        if (!blankPixelImage) blankPixelImage = CreateSinglePixelImage(glm::vec4(1));
        return blankPixelImage;
    }

    ImageViewPtr Renderer::CreateSinglePixelImage(glm::vec4 value) {
        uint8 data[4];
        for (size_t i = 0; i < 4; i++) {
            data[i] = (uint8_t)(255.0 * value[i]);
        }

        ImageCreateInfo imageInfo;
        imageInfo.imageType = vk::ImageType::e2D;
        imageInfo.usage = vk::ImageUsageFlagBits::eSampled;
        imageInfo.format = vk::Format::eR8G8B8A8Unorm;
        imageInfo.extent = vk::Extent3D(1, 1, 1);

        ImageViewCreateInfo viewInfo;
        viewInfo.defaultSampler = device.GetSampler(SamplerType::NearestTiled);
        auto imageView = device.CreateImageAndView(imageInfo, viewInfo, data, sizeof(data));
        return imageView;
    }
} // namespace sp::vulkan
