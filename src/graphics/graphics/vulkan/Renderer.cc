#include "Renderer.hh"

#include "assets/Model.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "graphics/vulkan/GuiRenderer.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/Image.hh"
#include "graphics/vulkan/core/Model.hh"
#include "graphics/vulkan/core/RenderGraph.hh"
#include "graphics/vulkan/core/Screenshot.hh"
#include "graphics/vulkan/core/Vertex.hh"

#ifdef SP_XR_SUPPORT
    #include "xr/XrSystem.hh"
#endif

namespace sp::vulkan {
    static const char *defaultWindowViewTarget = "GBuffer0";
    static CVar<string> CVarWindowViewTarget("r.WindowViewTarget",
        defaultWindowViewTarget,
        "Primary window's render target");

    Renderer::Renderer(DeviceContext &device) : device(device) {
        funcs.Register<string, string>("screenshot",
            "Save screenshot to <path>, optionally specifying an image <resource>",
            [&](string path, string resource) {
                QueueScreenshot(path, resource);
            });

        funcs.Register("listrendertargets", "List all render targets", [&]() {
            listRenderTargets = true;
        });
    }

    Renderer::~Renderer() {
        device->waitIdle();
    }

    void Renderer::LoadLightState(ecs::Lock<ecs::Read<ecs::Light, ecs::Transform>> lock) {
        int lightCount = 0;
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
            data.gelId = light.gelId;

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
        lights.gpuData.count = lightCount;
    }

    void Renderer::RenderFrame() {
        RenderGraph graph(device);

        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::Transform, ecs::Light, ecs::Renderable, ecs::View, ecs::XRView, ecs::Mirror>>();

        LoadLightState(lock);
        RenderShadowMaps(graph, lock);

        Tecs::Entity windowEntity = device.GetActiveView();

        if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
            auto view = windowEntity.Get<ecs::View>(lock);
            view.UpdateViewMatrix(lock, windowEntity);

            graph.Pass("ForwardPass")
                .Build([&](RenderGraphPassBuilder &builder) {
                    builder.ShaderRead("ShadowMapLinear");

                    RenderTargetDesc desc;
                    desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);

                    AttachmentInfo info;
                    info.loadOp = LoadOp::Clear;
                    info.storeOp = StoreOp::Store;
                    info.SetClearColor({0.0f, 1.0f, 0.0f, 1.0f});
                    desc.format = vk::Format::eR8G8B8A8Srgb;
                    desc.usage = vk::ImageUsageFlagBits::eColorAttachment;
                    builder.OutputColorAttachment(0, "GBuffer0", desc, info);

                    desc.format = vk::Format::eD24UnormS8Uint;
                    desc.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    builder.OutputDepthAttachment("GBufferDepth", desc, {LoadOp::Clear, StoreOp::DontCare});
                })
                .Execute([this, view, lock](RenderGraphResources &resources, CommandContext &cmd) {
                    cmd.SetShaders("test.vert", "test.frag");
                    cmd.SetTexture(0, 2, resources.GetRenderTarget("ShadowMapLinear")->ImageView());
                    cmd.UploadUniformData(0, 11, &this->lights.gpuData);

                    GPUViewState views[] = {{view}, {}};
                    cmd.UploadUniformData(0, 10, views, 2);

                    this->ForwardPass(cmd, view.visibilityMask, lock);
                });
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

            graph.Pass("XRForwardPass")
                .Build([&](RenderGraphPassBuilder &builder) {
                    RenderTargetDesc targetDesc;
                    targetDesc.extent = vk::Extent3D(firstView.extents.x, firstView.extents.y, 1);
                    targetDesc.arrayLayers = xrViews.size();

                    AttachmentInfo info;
                    info.loadOp = LoadOp::Clear;
                    info.storeOp = StoreOp::Store;
                    info.SetClearColor({0.0f, 1.0f, 0.0f, 1.0f});
                    targetDesc.format = vk::Format::eR8G8B8A8Srgb;
                    targetDesc.usage = vk::ImageUsageFlagBits::eColorAttachment;
                    builder.OutputColorAttachment(0, "XRColor", targetDesc, info);

                    targetDesc.format = vk::Format::eD24UnormS8Uint;
                    targetDesc.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    builder.OutputDepthAttachment("XRDepth", targetDesc, {LoadOp::Clear, StoreOp::DontCare});
                })
                .Execute([this, xrViews, visible, lock](RenderGraphResources &resources, CommandContext &cmd) {
                    auto viewState = cmd.AllocUniformData<GPUViewState>(0, 10, 2);
                    cmd.SetShaders("test.vert", "test.frag");
                    cmd.UploadUniformData(0, 11, &this->lights.gpuData);
                    cmd.SetTexture(0, 2, resources.GetRenderTarget("ShadowMapLinear")->ImageView());
                    this->ForwardPass(cmd, visible, lock);

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
                });

            graph.Pass("XRSubmit")
                .Build([&](RenderGraphPassBuilder &builder) {
                    builder.TransferRead("XRColor");
                    builder.RequirePass();
                })
                .Execute([this, xrViews, lock](RenderGraphResources &resources, DeviceContext &device) {
                    auto xrRenderTarget = resources.GetRenderTarget("XRColor");

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
                    auto res = builder.GetResourceByName(CVarWindowViewTarget.Get());
                    if (!res) {
                        Errorf("view target %s does not exist, defaulting to %s",
                            CVarWindowViewTarget.Get(),
                            defaultWindowViewTarget);
                        CVarWindowViewTarget.Set(defaultWindowViewTarget);
                        res = builder.GetResourceByName(defaultWindowViewTarget);
                    }

                    auto format = res.renderTargetDesc.format;
                    if (FormatComponentCount(format) == 4 && FormatByteSize(format) == 4) {
                        sourceID = res.id;
                    } else {
                        sourceID = VisualizeBuffer(graph, res.id);
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

        graph.Execute();
        EndFrame();
    }

    void Renderer::RenderShadowMaps(RenderGraph &graph, DrawLock lock) {
        if (lights.count == 0) return;

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
                    auto resource = builder.GetResourceByName(screenshotResource);
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
                    auto &res = resources.GetResourceByID(sourceID);
                    if (res.type == RenderGraphResource::Type::RenderTarget) {
                        auto target = resources.GetRenderTarget(res.id);
                        WriteScreenshot(device, screenshotPath, target->ImageView());
                    }
                });
        }
        pendingScreenshots.clear();
    }

    RenderGraphResourceID Renderer::VisualizeBuffer(RenderGraph &graph, RenderGraphResourceID sourceID) {
        RenderGraphResourceID targetID = ~0u, outputID;
        graph.Pass("VisualizeBuffer")
            .Build([&](RenderGraphPassBuilder &builder) {
                auto &res = builder.ShaderRead(sourceID);
                targetID = res.id;
                auto desc = res.renderTargetDesc;
                desc.format = vk::Format::eR8G8B8A8Srgb;
                outputID = builder.OutputColorAttachment(0, "", desc, {LoadOp::DontCare, StoreOp::Store}).id;
            })
            .Execute([targetID](RenderGraphResources &resources, CommandContext &cmd) {
                auto source = resources.GetRenderTarget(targetID)->ImageView();

                cmd.SetShaders("screen_cover.vert", "visualize_buffer.frag");

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
        activeModels.Tick();
    }

    void Renderer::SetDebugGui(GuiManager &gui) {
        debugGuiRenderer = make_unique<GuiRenderer>(device, gui);
    }

    void Renderer::QueueScreenshot(const string &path, const string &resource) {
        pendingScreenshots.push_back({path, resource});
    }
} // namespace sp::vulkan
