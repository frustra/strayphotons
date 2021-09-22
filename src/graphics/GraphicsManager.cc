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
        #include "graphics/vulkan/Renderer.hh"
        #include "graphics/vulkan/Screenshot.hh"
        #include "graphics/vulkan/Vertex.hh"
    #endif

    #include <algorithm>
    #include <cxxopts.hpp>
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

    static string ScreenshotVRPath;

    CFunc<string> CFuncQueueScreenshotVR("screenshotvr", "Save left eye screenshot to <path>", [](string path) {
        if (ScreenshotVRPath.empty()) {
            ScreenshotVRPath = path;
        } else {
            Logf("Can't save multiple screenshots on the same frame: %s, already saving %s",
                 path.c_str(),
                 ScreenshotVRPath.c_str());
        }
    });

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

            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::Transform, ecs::Renderable, ecs::View, ecs::XRView, ecs::Mirror>>();

            auto xrViews = lock.EntitiesWith<ecs::XRView>();
            xrRenderPoses.resize(xrViews.size());
            viewStateUniformBuffers.resize(1 + (xrViews.empty() ? 0 : 1));
            for (auto &bufferPtr : viewStateUniformBuffers) {
                if (!bufferPtr) {
                    bufferPtr = device.AllocateBuffer(sizeof(vulkan::ViewStateUniforms),
                                                      vk::BufferUsageFlagBits::eUniformBuffer,
                                                      VMA_MEMORY_USAGE_CPU_TO_GPU);
                }
            }
            auto currentViewState = viewStateUniformBuffers.begin();

            renderer->BeginFrame(lock);

            if (windowEntity && windowEntity.Has<ecs::View>(lock)) {
                auto view = windowEntity.Get<ecs::View>(lock);

                auto cmd = device.GetCommandContext();
                cmd->BeginRenderPass(device.SwapchainRenderPassInfo(true));

                cmd->SetUniformBuffer(0, 10, *currentViewState);

                renderer->RenderPass(cmd, view, lock);

                debugGuiRenderer->Render(cmd, vk::Rect2D{{0, 0}, cmd->GetFramebufferExtent()});

                cmd->EndRenderPass();

                glm::mat4 transform;
                if (windowEntity.Has<ecs::Transform>(lock)) {
                    transform = windowEntity.Get<ecs::Transform>(lock).GetGlobalTransform(lock);
                }
                vulkan::ViewStateUniforms viewState;
                viewState.view[0] = glm::inverse(transform);
                viewState.projection[0] = view.projMat;
                (*currentViewState)->CopyFrom(&viewState, 1);
                currentViewState++;

                device.Submit(cmd);
            }

        #ifdef SP_XR_SUPPORT
            if (xrSystem && xrViews.size() > 0) {
                auto cmd = device.GetCommandContext();
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

                if (!xrColor || xrColor->Image()->ArrayLayers() != xrViews.size() ||
                    xrColor->Extent() != vk::Extent3D(firstView.extents.x, firstView.extents.y, 1)) {

                    vk::ImageCreateInfo imageInfo;
                    imageInfo.imageType = vk::ImageType::e2D;
                    imageInfo.extent = vk::Extent3D(firstView.extents.x, firstView.extents.y, 1);
                    imageInfo.arrayLayers = xrViews.size();

                    imageInfo.format = vk::Format::eR8G8B8A8Srgb;
                    imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
                                      vk::ImageUsageFlagBits::eTransferSrc;
                    xrColor = device.CreateImageAndView(imageInfo, {});

                    imageInfo.format = vk::Format::eD24UnormS8Uint;
                    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                    xrDepth = device.CreateImageAndView(imageInfo, {});

                    cmd->ImageBarrier(xrDepth->Image(),
                                      vk::ImageLayout::eUndefined,
                                      vk::ImageLayout::eDepthAttachmentOptimal,
                                      vk::PipelineStageFlagBits::eBottomOfPipe,
                                      {},
                                      vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                      vk::AccessFlagBits::eDepthStencilAttachmentWrite);
                }

                cmd->ImageBarrier(xrColor->Image(),
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

                renderPassInfo.SetDepthStencilAttachment(xrDepth, vulkan::LoadOp::Clear, vulkan::StoreOp::DontCare);

                cmd->BeginRenderPass(renderPassInfo);

                cmd->SetUniformBuffer(0, 10, *currentViewState);

                renderer->RenderPass(cmd, firstView, lock); // TODO: pass individual fields instead of whole view

                cmd->EndRenderPass();

                cmd->ImageBarrier(xrColor->Image(),
                                  vk::ImageLayout::eColorAttachmentOptimal,
                                  vk::ImageLayout::eTransferSrcOptimal,
                                  vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                  {},
                                  vk::PipelineStageFlagBits::eTransfer,
                                  vk::AccessFlagBits::eTransferRead);

                vulkan::ViewStateUniforms viewState;

                for (size_t i = 0; i < xrViews.size(); i++) {
                    if (!xrViews[i].Has<ecs::View, ecs::XRView>(lock)) continue;
                    auto &view = xrViews[i].Get<ecs::View>(lock);
                    auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;

                    glm::mat4 transform;
                    if (xrViews[i].Has<ecs::Transform>(lock)) {
                        transform = xrViews[i].Get<ecs::Transform>(lock).GetGlobalTransform(lock);
                    }

                    if (xrSystem->GetPredictedViewPose(eye, xrRenderPoses[i])) {
                        viewState.view[i] = glm::inverse(transform * xrRenderPoses[i]);
                    } else {
                        viewState.view[i] = glm::inverse(transform);
                    }

                    viewState.projection[i] = view.projMat;
                }
                (*currentViewState)->CopyFrom(&viewState, 1);
                currentViewState++;

                device.Submit(cmd);

                for (size_t i = 0; i < xrViews.size(); i++) {
                    auto &eye = xrViews[i].Get<ecs::XRView>(lock).eye;
                    xrSystem->SubmitView(eye, xrRenderPoses[i], xrColor.get());
                }

                if (ScreenshotVRPath.size()) {
                    vulkan::WriteScreenshot(device, ScreenshotVRPath, xrColor);
                    ScreenshotVRPath = "";
                }
            }
        #endif

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
