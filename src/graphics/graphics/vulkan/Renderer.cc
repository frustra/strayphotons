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
    static CVar<string> CVarWindowViewTarget("r.WindowViewTarget",
                                             "GBuffer0",
                                             "Render target to view in the primary window");

    Renderer::Renderer(DeviceContext &device) : device(device) {
        funcs.Register<string>("screenshot",
                               "Save screenshot to <path>, optionally specifying an image <resource>",
                               [&](string pathAndResource) {
                                   std::istringstream ss(pathAndResource);
                                   string path, resource;
                                   ss >> path >> resource;
                                   TriggerScreenshot(path, resource);
                               });
    }

    Renderer::~Renderer() {
        device->waitIdle();
    }

    void Renderer::RenderFrame() {
        RenderGraph graph(device);
        auto renderer = this;

        auto lock = ecs::World.StartTransaction<
            ecs::Read<ecs::Name, ecs::Transform, ecs::Renderable, ecs::View, ecs::XRView, ecs::Mirror>>();

        Tecs::Entity windowEntity = device.GetActiveView();

        if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
            auto view = windowEntity.Get<ecs::View>(lock);
            view.UpdateViewMatrix(lock, windowEntity);

            graph.AddPass(
                "ForwardPass",
                [&](RenderGraphPassBuilder &builder) {
                    RenderTargetDesc desc;
                    desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);

                    desc.format = vk::Format::eR8G8B8A8Srgb;
                    desc.usage = vk::ImageUsageFlagBits::eColorAttachment;
                    builder.OutputRenderTarget("GBuffer0", desc);

                    desc.format = vk::Format::eD24UnormS8Uint;
                    desc.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    builder.OutputRenderTarget("GBufferDepth", desc);
                },
                [renderer, view, lock](RenderGraphResources &resources, CommandContext &cmd) {
                    auto gBuffer0 = resources.GetRenderTarget("GBuffer0")->ImageView();
                    auto depth = resources.GetRenderTarget("GBufferDepth")->ImageView();

                    RenderPassInfo info;
                    glm::vec4 clear = {0.0f, 1.0f, 0.0f, 1.0f};
                    info.PushColorAttachment(gBuffer0, LoadOp::Clear, StoreOp::Store, clear);
                    info.SetDepthStencilAttachment(depth, LoadOp::Clear, StoreOp::DontCare);
                    cmd.BeginRenderPass(info);

                    ViewStateUniforms viewState;
                    viewState.view[0] = view.viewMat;
                    viewState.projection[0] = view.projMat;
                    cmd.UploadUniformData(0, 10, &viewState);

                    cmd.SetShaders("test.vert", "test.frag");
                    renderer->ForwardPass(cmd, view.visibilityMask, lock, [](auto lock, Tecs::Entity &ent) {});

                    cmd.EndRenderPass();
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
            auto xrRenderPosePtr = xrRenderPoses.data();
            auto forwardPassViewMask = firstView.visibilityMask;

            graph.AddPass(
                "XRForwardPass",
                [&](RenderGraphPassBuilder &builder) {
                    RenderTargetDesc targetDesc;
                    targetDesc.extent = vk::Extent3D(firstView.extents.x, firstView.extents.y, 1);
                    targetDesc.arrayLayers = xrViews.size();

                    targetDesc.format = vk::Format::eR8G8B8A8Srgb;
                    targetDesc.usage = vk::ImageUsageFlagBits::eColorAttachment;
                    builder.OutputRenderTarget("XRColor", targetDesc);

                    targetDesc.format = vk::Format::eD24UnormS8Uint;
                    targetDesc.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    builder.OutputRenderTarget("XRDepth", targetDesc);
                },
                [renderer, xrViews, xrRenderPosePtr, forwardPassViewMask, lock](RenderGraphResources &resources,
                                                                                CommandContext &cmd) {
                    auto xrColor = resources.GetRenderTarget("XRColor")->ImageView();
                    auto xrDepth = resources.GetRenderTarget("XRDepth")->ImageView();

                    RenderPassInfo renderPassInfo;
                    renderPassInfo.PushColorAttachment(xrColor,
                                                       LoadOp::Clear,
                                                       StoreOp::Store,
                                                       {0.0f, 1.0f, 0.0f, 1.0f});

                    renderPassInfo.SetDepthStencilAttachment(xrDepth, LoadOp::Clear, StoreOp::DontCare);

                    cmd.BeginRenderPass(renderPassInfo);

                    auto viewState = cmd.AllocUniformData<ViewStateUniforms>(0, 10);

                    cmd.SetShaders("test.vert", "test.frag");
                    renderer->ForwardPass(cmd, forwardPassViewMask, lock, [](auto lock, Tecs::Entity &ent) {});

                    cmd.EndRenderPass();

                    for (size_t i = 0; i < xrViews.size(); i++) {
                        if (!xrViews[i].Has<ecs::View, ecs::XRView>(lock)) continue;
                        auto view = xrViews[i].Get<ecs::View>(lock);
                        auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;

                        view.UpdateViewMatrix(lock, xrViews[i]);

                        if (renderer->xrSystem->GetPredictedViewPose(eye, xrRenderPosePtr[i])) {
                            viewState->view[i] = glm::inverse(xrRenderPosePtr[i]) * view.viewMat;
                        } else {
                            viewState->view[i] = view.viewMat;
                        }

                        viewState->projection[i] = view.projMat;
                    }
                });

            graph.AddPass(
                "XRSubmit",
                [&](RenderGraphPassBuilder &builder) {
                    builder.TransferRead("XRColor");
                },
                [renderer, xrViews, xrRenderPosePtr, lock](RenderGraphResources &resources, CommandContext &cmd) {
                    auto xrRenderTarget = resources.GetRenderTarget("XRColor");

                    cmd.AfterSubmit([renderer, xrRenderTarget, xrViews, xrRenderPosePtr, lock]() {
                        for (size_t i = 0; i < xrViews.size(); i++) {
                            auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;
                            renderer->xrSystem->SubmitView(eye, xrRenderPosePtr[i], xrRenderTarget->ImageView().get());
                        }
                    });
                });
        }
#endif

        auto swapchainImage = device.SwapchainImageView();
        if (windowEntity && windowEntity.Has<ecs::View>(lock) && swapchainImage) {
            auto view = windowEntity.Get<ecs::View>(lock);
            auto debugGuiRenderer = this->debugGuiRenderer.get();
            auto windowViewTarget = CVarWindowViewTarget.Get();

            graph.AddPass(
                "WindowFinalOutput",
                [&](RenderGraphPassBuilder &builder) {
                    builder.ShaderRead(windowViewTarget);

                    RenderTargetDesc desc;
                    desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);
                    desc.format = vk::Format::eR8G8B8A8Srgb;
                    desc.usage = vk::ImageUsageFlagBits::eColorAttachment;
                    builder.OutputRenderTarget("WindowFinalOutput", desc);
                },
                [debugGuiRenderer, windowViewTarget](RenderGraphResources &resources, CommandContext &cmd) {
                    auto source = resources.GetRenderTarget(windowViewTarget)->ImageView();
                    auto color = resources.GetRenderTarget("WindowFinalOutput")->ImageView();

                    RenderPassInfo info;
                    info.PushColorAttachment(color, LoadOp::Clear, StoreOp::Store, {0, 1, 0, 1});
                    cmd.BeginRenderPass(info);

                    cmd.SetTexture(0, 0, source);
                    cmd.DrawScreenCover(source);

                    if (debugGuiRenderer) {
                        debugGuiRenderer->Render(cmd, vk::Rect2D{{0, 0}, cmd.GetFramebufferExtent()});
                    }

                    cmd.EndRenderPass();
                });

            graph.SetTargetImageView("WindowFinalOutput", swapchainImage);
        }

        AddScreenshotPasses(graph);
        graph.Execute();
        EndFrame();
    }

    void Renderer::AddScreenshotPasses(RenderGraph &graph) {
        for (auto &pending : pendingScreenshots) {
            auto screenshotPath = pending.first;
            auto screenshotResource = pending.second;
            if (screenshotResource.empty()) screenshotResource = CVarWindowViewTarget.Get();

            graph.AddPass(
                "Screenshot",
                [&](RenderGraphPassBuilder &builder) {
                    auto resource = builder.GetResourceByName(screenshotResource);
                    if (resource.type != RenderGraphResource::Type::RenderTarget) {
                        Errorf("Can't screenshot \"%s\": invalid resource", screenshotResource);
                    } else {
                        builder.TransferRead(resource.id);
                    }
                },
                [screenshotPath, screenshotResource](RenderGraphResources &resources, CommandContext &cmd) {
                    auto &res = resources.GetResourceByName(screenshotResource);
                    if (res.type == RenderGraphResource::Type::RenderTarget) {
                        auto target = resources.GetRenderTarget(res.id);
                        WriteScreenshot(cmd.Device(), screenshotPath, target->ImageView());
                    }
                });
        }
        pendingScreenshots.clear();
    }

    void Renderer::ForwardPass(CommandContext &cmd,
                               ecs::Renderable::VisibilityMask viewMask,
                               DrawLock lock,
                               const PreDrawFunc &preDraw) {
        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform>(lock)) {
                if (ent.Has<ecs::Mirror>(lock)) continue;
                DrawEntity(cmd, viewMask, lock, ent, preDraw);
            }
        }

        for (Tecs::Entity &ent : lock.EntitiesWith<ecs::Renderable>()) {
            if (ent.Has<ecs::Renderable, ecs::Transform, ecs::Mirror>(lock)) {
                DrawEntity(cmd, viewMask, lock, ent, preDraw);
            }
        }
    }

    void Renderer::DrawEntity(CommandContext &cmd,
                              ecs::Renderable::VisibilityMask viewMask,
                              DrawLock lock,
                              Tecs::Entity &ent,
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

        model->Draw(cmd, modelMat); // TODO pass and use comp.model->bones
    }

    void Renderer::EndFrame() {
        activeModels.Tick();
    }

    void Renderer::SetDebugGui(GuiManager &gui) {
        debugGuiRenderer = make_unique<GuiRenderer>(device, gui);
    }

    void Renderer::TriggerScreenshot(const string &path, const string &resource) {
        pendingScreenshots.push_back({path, resource});
    }
} // namespace sp::vulkan
