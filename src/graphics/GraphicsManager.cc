#ifdef SP_GRAPHICS_SUPPORT
    #include "GraphicsManager.hh"

    #include "core/CVar.hh"
    #include "core/Logging.hh"
    #include "ecs/EcsImpl.hh"
    #include "game/Game.hh"
    #include "graphics/core/GraphicsContext.hh"

    #ifdef SP_GRAPHICS_SUPPORT_GL
        #include "graphics/opengl/GlfwGraphicsContext.hh"
        #include "graphics/opengl/PerfTimer.hh"
        #include "graphics/opengl/RenderTargetPool.hh"
        #include "graphics/opengl/gui/ProfilerGui.hh"
        #include "graphics/opengl/voxel_renderer/VoxelRenderer.hh"
        #include "input/glfw/GlfwInputHandler.hh"
    #endif
    #ifdef SP_GRAPHICS_SUPPORT_VK
        #include "graphics/vulkan/CommandContext.hh"
        #include "graphics/vulkan/DeviceContext.hh"
        #include "graphics/vulkan/GuiRenderer.hh"
        #include "graphics/vulkan/Image.hh"
        #include "graphics/vulkan/RenderGraph.hh"
        #include "graphics/vulkan/Renderer.hh"
        #include "graphics/vulkan/Screenshot.hh"
        #include "graphics/vulkan/Vertex.hh"
    #endif

    #include <algorithm>
    #include <cxxopts.hpp>
    #include <glm/gtc/matrix_access.hpp>
    #include <iostream>
    #include <optional>
    #include <system_error>

    #ifdef SP_XR_SUPPORT
        #include <openvr.h>
    #endif

namespace sp {
    GraphicsManager::GraphicsManager(Game *game) : game(game) {
    #ifdef SP_GRAPHICS_SUPPORT_GL
        Logf("Graphics starting up (OpenGL)");
    #endif
    #ifdef SP_GRAPHICS_SUPPORT_VK
        Logf("Graphics starting up (Vulkan)");
    #endif
    }

    GraphicsManager::~GraphicsManager() {
        if (context) context->WaitIdle();
    }

    static string ScreenshotPath, ScreenshotResource;

    CFunc<string> CFuncQueueScreenshotResource(
        "screenshot",
        "Save screenshot of <resource> to <path>",
        [](string resourceAndPath) {
            std::istringstream ss(resourceAndPath);
            string resource, path;
            ss >> resource >> path;
            if (ScreenshotPath.empty()) {
                ScreenshotPath = path;
                ScreenshotResource = resource;
            } else {
                Logf("Can't save multiple screenshots on the same frame: %s, already saving %s",
                     path.c_str(),
                     ScreenshotPath.c_str());
            }
        });

    static CVar<string> CVarWindowViewTarget("r.WindowViewTarget",
                                             "GBuffer0",
                                             "Render target to view in the primary window");

    void GraphicsManager::Init() {
        if (context) { throw "already an active context"; }

    #if SP_GRAPHICS_SUPPORT_GL
        auto glfwContext = new GlfwGraphicsContext();
        context.reset(glfwContext);

        GLFWwindow *window = glfwContext->GetWindow();
    #endif

    #if SP_GRAPHICS_SUPPORT_VK
        bool enableValidationLayers = game->options.count("with-validation-layers") > 0;

        auto vkContext = new vulkan::DeviceContext(enableValidationLayers);
        context.reset(vkContext);

        GLFWwindow *window = vkContext->GetWindow();
    #endif

        if (window != nullptr) game->glfwInputHandler = make_unique<GlfwInputHandler>(*window);

        if (game->options.count("size")) {
            std::istringstream ss(game->options["size"].as<string>());
            glm::ivec2 size;
            ss >> size.x >> size.y;

            if (size.x > 0 && size.y > 0) { CVarWindowSize.Set(size); }
        }

    #ifdef SP_GRAPHICS_SUPPORT_GL
        if (renderer) { throw "already an active renderer"; }

        renderer = make_unique<VoxelRenderer>(*glfwContext, timer);

        profilerGui = make_shared<ProfilerGui>(timer);
        if (game->debugGui) { game->debugGui->Attach(profilerGui); }

        renderer->PrepareGuis(game->debugGui.get(), game->menuGui.get());

        renderer->Prepare();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        if (renderer) { throw "already an active renderer"; }

        {
            auto lock = ecs::World.StartTransaction<ecs::AddRemove>();
            renderer = make_unique<vulkan::Renderer>(lock, *vkContext);
        }

        renderer->Prepare();

        debugGuiRenderer = make_unique<vulkan::GuiRenderer>(*vkContext, *game->debugGui.get());
    #endif
    }

    bool GraphicsManager::HasActiveContext() {
        return context && !context->ShouldClose();
    }

    bool GraphicsManager::Frame() {
    #if SP_GRAPHICS_SUPPORT_GL || SP_GRAPHICS_SUPPORT_VK
        if (!context) throw "no active context";
        if (!HasActiveContext()) return false;
        if (!renderer) throw "no active renderer";
    #endif

        if (game->debugGui) game->debugGui->BeforeFrame();
        if (game->menuGui) game->menuGui->BeforeFrame();

        Tecs::Entity windowEntity = context->GetActiveView();
        if (windowEntity) {
            auto lock = ecs::World.StartTransaction<ecs::Write<ecs::View>>();

            if (windowEntity.Has<ecs::View>(lock)) {
                auto &view = windowEntity.Get<ecs::View>(lock);
                view.visibilityMask.set(ecs::Renderable::VISIBLE_DIRECT_CAMERA);
                context->PrepareWindowView(view);
            }
        }

    #ifdef SP_XR_SUPPORT
        auto xrSystem = game->xr.GetXrSystem();

        if (xrSystem) xrSystem->WaitFrame();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_GL
        timer.StartFrame();
        context->BeginFrame();

        GlfwGraphicsContext *glContext = dynamic_cast<GlfwGraphicsContext *>(context.get());
        Assert(glContext != nullptr, "GraphicsManager::Frame(): GraphicsContext is not a GlfwGraphicsContext");

        {
            RenderPhase phase("Frame", timer);

            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::Transform, ecs::XRView>,
                ecs::Write<ecs::Renderable, ecs::View, ecs::Light, ecs::LightSensor, ecs::Mirror, ecs::VoxelArea>>();
            renderer->BeginFrame(lock);

            if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
                auto &view = windowEntity.Get<ecs::View>(lock);
                view.UpdateViewMatrix(lock, windowEntity);
                renderer->RenderPass(view, lock);
            }

        #ifdef SP_XR_SUPPORT
            if (xrSystem) {
                RenderPhase xrPhase("XrViews", timer);

                auto xrViews = lock.EntitiesWith<ecs::XRView>();
                xrRenderTargets.resize(xrViews.size());
                xrRenderPoses.resize(xrViews.size());
                for (size_t i = 0; i < xrViews.size(); i++) {
                    if (!xrViews[i].Has<ecs::View, ecs::XRView>(lock)) continue;
                    auto &view = xrViews[i].Get<ecs::View>(lock);
                    auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;

                    RenderPhase xrSubPhase("XrView", timer);

                    RenderTargetDesc desc = {PF_SRGB8_A8, view.extents};
                    if (!xrRenderTargets[i] || xrRenderTargets[i]->GetDesc() != desc) {
                        xrRenderTargets[i] = glContext->GetRenderTarget(desc);
                    }

                    view.UpdateViewMatrix(lock, xrViews[i]);
                    if (xrSystem->GetPredictedViewPose(eye, xrRenderPoses[i])) {
                        view.viewMat = glm::inverse(xrRenderPoses[i]) * view.viewMat;
                        view.invViewMat = view.invViewMat * xrRenderPoses[i];
                    }

                    renderer->RenderPass(view, lock, xrRenderTargets[i].get());
                }

                for (size_t i = 0; i < xrViews.size(); i++) {
                    if (!xrViews[i].Has<ecs::View, ecs::XRView>(lock)) continue;
                    RenderPhase xrSubPhase("XrViewSubmit", timer);

                    auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;
                    xrSystem->SubmitView(eye, xrRenderPoses[i], xrRenderTargets[i]->GetTexture());
                }
            }
        #endif

            renderer->EndFrame();
        }

        context->SwapBuffers();

        timer.EndFrame();
        context->EndFrame();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        // timer.StartFrame();
        context->BeginFrame();

        {
            // RenderPhase phase("Frame", timer);
            auto &device = *dynamic_cast<vulkan::DeviceContext *>(context.get());
            vulkan::RenderGraph graph(device);

            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::Transform, ecs::Renderable, ecs::View, ecs::XRView, ecs::Mirror>>();

            auto xrViews = lock.EntitiesWith<ecs::XRView>();
            xrRenderPoses.resize(xrViews.size());

            auto renderer = this->renderer.get();
            renderer->BeginFrame(lock);

            if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
                auto view = windowEntity.Get<ecs::View>(lock);
                view.UpdateViewMatrix(lock, windowEntity);

                struct ForwardPassData {
                    vulkan::RenderGraphResource gBuffer0, depth;
                };

                auto forwardPassData = graph.AddPass<ForwardPassData>(
                    "ForwardPass",
                    [&](vulkan::RenderGraphPassBuilder &builder, ForwardPassData &data) {
                        vulkan::RenderTargetDesc desc;
                        desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);

                        desc.format = vk::Format::eR8G8B8A8Srgb;
                        desc.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
                                     vk::ImageUsageFlagBits::eTransferSrc;
                        data.gBuffer0 = builder.OutputRenderTarget("GBuffer0", desc);

                        desc.format = vk::Format::eD24UnormS8Uint;
                        desc.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                        data.depth = builder.OutputRenderTarget("GBufferDepth", desc);
                    },
                    [renderer, view, lock](vulkan::RenderGraphResources &resources,
                                           vulkan::CommandContext &cmd,
                                           const ForwardPassData &data) {
                        auto gBuffer0 = resources.GetRenderTarget(data.gBuffer0)->ImageView();
                        auto depth = resources.GetRenderTarget(data.depth)->ImageView();

                        cmd.ImageBarrier(depth->Image(),
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eDepthAttachmentOptimal,
                                         vk::PipelineStageFlagBits::eBottomOfPipe,
                                         {},
                                         vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                         vk::AccessFlagBits::eDepthStencilAttachmentWrite);

                        cmd.ImageBarrier(gBuffer0->Image(),
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eColorAttachmentOptimal,
                                         vk::PipelineStageFlagBits::eTransfer,
                                         {},
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         vk::AccessFlagBits::eColorAttachmentWrite);

                        vulkan::RenderPassInfo info;
                        glm::vec4 clear = {0.0f, 1.0f, 0.0f, 1.0f};
                        info.PushColorAttachment(gBuffer0, vulkan::LoadOp::Clear, vulkan::StoreOp::Store, clear);
                        info.SetDepthStencilAttachment(depth, vulkan::LoadOp::Clear, vulkan::StoreOp::DontCare);
                        cmd.BeginRenderPass(info);

                        vulkan::ViewStateUniforms viewState;
                        viewState.view[0] = view.viewMat;
                        viewState.projection[0] = view.projMat;
                        cmd.UploadUniformData(0, 10, &viewState);

                        renderer->RenderPass(cmd, view, lock);

                        cmd.EndRenderPass();
                    });
            }

        #ifdef SP_XR_SUPPORT
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

                struct XRForwardPassData {
                    vulkan::RenderGraphResource color, depth;
                };

                auto xrRenderPosePtr = xrRenderPoses.data();

                auto xrForwardPassData = graph.AddPass<XRForwardPassData>(
                    "XRForwardPass",
                    [&](vulkan::RenderGraphPassBuilder &builder, XRForwardPassData &data) {
                        vulkan::RenderTargetDesc targetDesc;
                        targetDesc.extent = vk::Extent3D(firstView.extents.x, firstView.extents.y, 1);
                        targetDesc.arrayLayers = xrViews.size();

                        targetDesc.format = vk::Format::eR8G8B8A8Srgb;
                        targetDesc.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
                                           vk::ImageUsageFlagBits::eTransferSrc;
                        data.color = builder.OutputRenderTarget("XRColor", targetDesc);

                        targetDesc.format = vk::Format::eD24UnormS8Uint;
                        targetDesc.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                        data.depth = builder.OutputRenderTarget("XRDepth", targetDesc);
                    },
                    [renderer, xrViews, xrSystem, xrRenderPosePtr, lock](vulkan::RenderGraphResources &resources,
                                                                         vulkan::CommandContext &cmd,
                                                                         const XRForwardPassData &data) {
                        auto xrColor = resources.GetRenderTarget(data.color)->ImageView();
                        auto xrDepth = resources.GetRenderTarget(data.depth)->ImageView();

                        cmd.ImageBarrier(xrDepth->Image(),
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eDepthAttachmentOptimal,
                                         vk::PipelineStageFlagBits::eBottomOfPipe,
                                         {},
                                         vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                         vk::AccessFlagBits::eDepthStencilAttachmentWrite);

                        cmd.ImageBarrier(xrColor->Image(),
                                         vk::ImageLayout::eUndefined,
                                         vk::ImageLayout::eColorAttachmentOptimal,
                                         vk::PipelineStageFlagBits::eTransfer,
                                         {},
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         vk::AccessFlagBits::eColorAttachmentWrite);

                        vulkan::RenderPassInfo renderPassInfo;
                        renderPassInfo.state.multiviewAttachments = 0b11;
                        renderPassInfo.state.multiviewCorrelations = 0b11;
                        renderPassInfo.PushColorAttachment(xrColor,
                                                           vulkan::LoadOp::Clear,
                                                           vulkan::StoreOp::Store,
                                                           {0.0f, 1.0f, 0.0f, 1.0f});

                        renderPassInfo.SetDepthStencilAttachment(xrDepth,
                                                                 vulkan::LoadOp::Clear,
                                                                 vulkan::StoreOp::DontCare);

                        cmd.BeginRenderPass(renderPassInfo);

                        auto viewState = cmd.AllocUniformData<vulkan::ViewStateUniforms>(0, 10);

                        // TODO: pass individual fields instead of whole view
                        renderer->RenderPass(cmd, xrViews[0].Get<ecs::View>(lock), lock);

                        cmd.EndRenderPass();

                        for (size_t i = 0; i < xrViews.size(); i++) {
                            if (!xrViews[i].Has<ecs::View, ecs::XRView>(lock)) continue;
                            auto view = xrViews[i].Get<ecs::View>(lock);
                            auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;

                            view.UpdateViewMatrix(lock, xrViews[i]);

                            if (xrSystem->GetPredictedViewPose(eye, xrRenderPosePtr[i])) {
                                viewState->view[i] = glm::inverse(xrRenderPosePtr[i]) * view.viewMat;
                            } else {
                                viewState->view[i] = view.viewMat;
                            }

                            viewState->projection[i] = view.projMat;
                        }
                    });

                struct XRSubmitData {
                    vulkan::RenderGraphResource color;
                };

                graph.AddPass<XRSubmitData>(
                    "XRSubmit",
                    [&](vulkan::RenderGraphPassBuilder &builder, XRSubmitData &data) {
                        data.color = builder.Read(xrForwardPassData.color);
                    },
                    [xrSystem, xrViews, xrRenderPosePtr, lock](vulkan::RenderGraphResources &resources,
                                                               vulkan::CommandContext &cmd,
                                                               const XRSubmitData &data) {
                        auto xrRenderTarget = resources.GetRenderTarget(data.color);
                        cmd.ImageBarrier(xrRenderTarget->ImageView()->Image(),
                                         vk::ImageLayout::eColorAttachmentOptimal,
                                         vk::ImageLayout::eTransferSrcOptimal,
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         {},
                                         vk::PipelineStageFlagBits::eTransfer,
                                         vk::AccessFlagBits::eTransferRead);

                        cmd.AfterSubmit([xrRenderTarget, xrViews, xrSystem, xrRenderPosePtr, lock]() {
                            for (size_t i = 0; i < xrViews.size(); i++) {
                                auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;
                                xrSystem->SubmitView(eye, xrRenderPosePtr[i], xrRenderTarget->ImageView().get());
                            }
                        });
                    });
            }
        #endif

            if (ScreenshotPath.size()) {
                struct ScreenshotData {
                    vulkan::RenderGraphResource target;
                    string path;
                };
                graph.AddPass<ScreenshotData>(
                    "Screenshot",
                    [&](vulkan::RenderGraphPassBuilder &builder, ScreenshotData &data) {
                        auto resource = builder.GetResourceByName(ScreenshotResource);
                        if (resource.type != vulkan::RenderGraphResource::Type::RenderTarget) {
                            Errorf("Can't screenshot \"%s\": invalid resource", ScreenshotResource);
                        } else {
                            data.target = builder.Read(resource);
                            data.path = ScreenshotPath;
                        }
                    },
                    [](vulkan::RenderGraphResources &resources,
                       vulkan::CommandContext &cmd,
                       const ScreenshotData &data) {
                        if (data.target.type == vulkan::RenderGraphResource::Type::RenderTarget) {
                            auto target = resources.GetRenderTarget(data.target);
                            vulkan::WriteScreenshot(cmd.Device(), data.path, target->ImageView());
                        }
                    });
                ScreenshotPath = "";
            }

            if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
                auto view = windowEntity.Get<ecs::View>(lock);
                struct FinalOutputData {
                    vulkan::RenderGraphResource source;
                    vulkan::RenderGraphResource target;
                };

                auto debugGuiRenderer = this->debugGuiRenderer.get();
                graph.AddPass<FinalOutputData>(
                    "WindowFinalOutput",
                    [&](vulkan::RenderGraphPassBuilder &builder, FinalOutputData &data) {
                        data.source = builder.Read(builder.GetResourceByName(CVarWindowViewTarget.Get()));

                        vulkan::RenderTargetDesc desc;
                        desc.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);
                        desc.format = vk::Format::eR8G8B8A8Srgb;
                        desc.usage = vk::ImageUsageFlagBits::eColorAttachment;
                        data.target = builder.OutputRenderTarget("WindowFinalOutput", desc);
                    },
                    [debugGuiRenderer](vulkan::RenderGraphResources &resources,
                                       vulkan::CommandContext &cmd,
                                       const FinalOutputData &data) {
                        auto source = resources.GetRenderTarget(data.source)->ImageView();

                        cmd.ImageBarrier(source->Image(),
                                         source->Image()->LastLayout(),
                                         vk::ImageLayout::eShaderReadOnlyOptimal,
                                         vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                         vk::AccessFlagBits::eColorAttachmentWrite,
                                         vk::PipelineStageFlagBits::eFragmentShader,
                                         vk::AccessFlagBits::eShaderRead);

                        auto color = resources.GetRenderTarget(data.target)->ImageView();

                        vulkan::RenderPassInfo info;
                        info.PushColorAttachment(color, vulkan::LoadOp::Clear, vulkan::StoreOp::Store, {0, 1, 0, 1});
                        cmd.BeginRenderPass(info);

                        cmd.SetTexture(0, 0, source);
                        cmd.DrawScreenCover(source);
                        debugGuiRenderer->Render(cmd, vk::Rect2D{{0, 0}, cmd.GetFramebufferExtent()});

                        cmd.EndRenderPass();
                    });

                graph.SetTargetImageView("WindowFinalOutput", device.SwapchainImageView());
            }
            graph.Execute();

            renderer->EndFrame();
        }

        context->SwapBuffers();

        // timer.EndFrame();
        context->EndFrame();
    #endif

        return true;
    }

    GraphicsContext *GraphicsManager::GetContext() {
        return context.get();
    }

    void GraphicsManager::RenderLoading() {
    #ifdef SP_GRAPHICS_SUPPORT_GL
        if (!renderer) return;

        auto lock = ecs::World.StartTransaction<ecs::Read<ecs::View>>();

        auto &windowEntity = context->GetActiveView();
        if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
            auto windowView = windowEntity.Get<ecs::View>(lock);
            windowView.blend = true;
            windowView.clearMode.reset();

            renderer->RenderLoading(windowView);
            context->SwapBuffers();
        }

    // TODO: clear the XR scene to drop back to the compositor while we load
    #endif
    }
} // namespace sp

#endif
