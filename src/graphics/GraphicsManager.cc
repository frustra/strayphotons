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
        #include "graphics/vulkan/Vertex.hh"
    #endif

    #include <algorithm>
    #include <cxxopts.hpp>
    #include <iostream>
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

        std::vector<ecs::View> cameraViews;
        std::vector<ecs::View> shadowViews;
        std::vector<std::pair<ecs::View, ecs::XRView>> xrViews;
        {
            auto lock = ecs::World.StartTransaction<ecs::Read<ecs::Transform, ecs::Light, ecs::XRView>,
                                                    ecs::Write<ecs::View>>();

            auto &windowEntity = context->GetActiveView();
            if (windowEntity) {
                if (windowEntity.Has<ecs::View>(lock)) {
                    auto &windowView = windowEntity.Get<ecs::View>(lock);
                    windowView.visibilityMask.set(ecs::Renderable::VISIBLE_DIRECT_CAMERA);
                    context->PrepareWindowView(windowView);
                }
            }

            for (auto &e : lock.EntitiesWith<ecs::Light>()) {
                if (e.Has<ecs::Light, ecs::View>(lock)) {
                    auto &view = e.Get<ecs::View>(lock);
                    view.visibilityMask.set(ecs::Renderable::VISIBLE_LIGHTING_SHADOW);
                }
            }

            for (auto &e : lock.EntitiesWith<ecs::View>()) {
                auto &view = e.Get<ecs::View>(lock);
                view.UpdateMatrixCache(lock, e);
                if (view.visibilityMask[ecs::Renderable::VISIBLE_DIRECT_CAMERA]) {
                    cameraViews.emplace_back(view);
                } else if (view.visibilityMask[ecs::Renderable::VISIBLE_DIRECT_EYE] && e.Has<ecs::XRView>(lock)) {
                    xrViews.emplace_back(view, e.Get<ecs::XRView>(lock));
                } else if (view.visibilityMask[ecs::Renderable::VISIBLE_LIGHTING_SHADOW]) {
                    shadowViews.emplace_back(view);
                }
            }
        }

    #ifdef SP_XR_SUPPORT
        auto xrSystem = game->xr.GetXrSystem();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_GL
        timer.StartFrame();
        context->BeginFrame();

        GlfwGraphicsContext *glContext = dynamic_cast<GlfwGraphicsContext *>(context.get());
        Assert(glContext != nullptr, "GraphicsManager::Frame(): GraphicsContext is not a GlfwGraphicsContext");

        {
            RenderPhase phase("Frame", timer);

            auto lock = ecs::World.StartTransaction<
                ecs::Read<ecs::Name, ecs::Transform>,
                ecs::Write<ecs::Renderable, ecs::View, ecs::Light, ecs::LightSensor, ecs::Mirror, ecs::VoxelArea>>();
            renderer->BeginFrame(lock);

            for (auto &view : cameraViews) {
                renderer->RenderPass(view, lock);
            }

        #ifdef SP_XR_SUPPORT
            if (xrSystem) {
                RenderPhase xrPhase("XrViews", timer);

                xrRenderTargets.resize(xrViews.size());
                for (size_t i = 0; i < xrViews.size(); i++) {
                    auto &view = xrViews[i].first;

                    RenderPhase xrSubPhase("XrView", timer);

                    RenderTargetDesc desc = {PF_SRGB8_A8, view.extents};
                    if (!xrRenderTargets[i].renderTarget || xrRenderTargets[i].renderTarget->GetDesc() != desc) {
                        xrRenderTargets[i].renderTarget = glContext->GetRenderTarget(desc);
                    }

                    renderer->RenderPass(view, lock, xrRenderTargets[i].renderTarget.get());
                }

                for (size_t i = 0; i < xrViews.size(); i++) {
                    RenderPhase xrSubPhase("XrViewSubmit", timer);

                    xrSystem->SubmitView(xrViews[i].second.eye, xrRenderTargets[i].renderTarget->GetTexture());
                }
            }
        #endif

            renderer->EndFrame();
        }

        context->SwapBuffers();

        #ifdef SP_XR_SUPPORT
        if (xrSystem) {
            RenderPhase xrPhase("XrWaitFrame", timer);
            xrSystem->WaitFrame();
        }
        #endif

        timer.EndFrame();
        context->EndFrame();
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        // timer.StartFrame();
        context->BeginFrame();

        {
            // RenderPhase phase("Frame", timer);
            auto &device = *dynamic_cast<vulkan::DeviceContext *>(context.get());

            auto lock =
                ecs::World
                    .StartTransaction<ecs::Read<ecs::Name, ecs::Transform, ecs::Renderable, ecs::View, ecs::Mirror>>();
            renderer->BeginFrame(lock);

            auto cmd = device.GetCommandContext();
            cmd->BeginRenderPass(device.SwapchainRenderPassInfo(true));

            for (auto &view : cameraViews) {
                renderer->RenderPass(cmd, view, lock);
            }

            debugGuiRenderer->Render(cmd, vk::Rect2D{{0, 0}, cmd->GetFramebufferExtent()});

            cmd->EndRenderPass();
            device.Submit(cmd);

        #ifdef SP_XR_SUPPORT
            if (xrSystem) {
                xrRenderTargets.resize(xrViews.size());
                for (size_t i = 0; i < xrViews.size(); i++) {
                    auto &view = xrViews[i].first;

                    cmd = device.GetCommandContext();

                    if (!xrRenderTargets[i].color || xrRenderTargets[i].color->GetWidth() != view.extents.x ||
                        xrRenderTargets[i].color->GetHeight() != view.extents.y) {

                        vk::ImageCreateInfo imageInfo;
                        imageInfo.imageType = vk::ImageType::e2D;
                        imageInfo.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);

                        imageInfo.format = vk::Format::eR8G8B8A8Srgb;
                        imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
                                          vk::ImageUsageFlagBits::eTransferSrc;
                        xrRenderTargets[i].color = device.CreateImageAndView(imageInfo, {});

                        imageInfo.format = vk::Format::eD24UnormS8Uint;
                        imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                        xrRenderTargets[i].depth = device.CreateImageAndView(imageInfo, {});

                        cmd->ImageBarrier(xrRenderTargets[i].depth->Image(),
                                          vk::ImageLayout::eUndefined,
                                          vk::ImageLayout::eDepthAttachmentOptimal,
                                          vk::PipelineStageFlagBits::eBottomOfPipe,
                                          {},
                                          vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                          vk::AccessFlagBits::eDepthStencilAttachmentWrite);
                    }

                    cmd->ImageBarrier(xrRenderTargets[i].color->Image(),
                                      vk::ImageLayout::eUndefined,
                                      vk::ImageLayout::eColorAttachmentOptimal,
                                      vk::PipelineStageFlagBits::eTransfer,
                                      {},
                                      vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                      vk::AccessFlagBits::eColorAttachmentWrite);

                    vulkan::RenderPassInfo renderPassInfo;
                    renderPassInfo.PushColorAttachment(xrRenderTargets[i].color,
                                                       vulkan::LoadOp::Clear,
                                                       vulkan::StoreOp::Store,
                                                       {0.0f, 1.0f, 0.0f, 1.0f});

                    renderPassInfo.SetDepthStencilAttachment(xrRenderTargets[i].depth,
                                                             vulkan::LoadOp::Clear,
                                                             vulkan::StoreOp::DontCare);

                    cmd->BeginRenderPass(renderPassInfo);

                    renderer->RenderPass(cmd, view, lock);

                    cmd->EndRenderPass();

                    cmd->ImageBarrier(xrRenderTargets[i].color->Image(),
                                      vk::ImageLayout::eColorAttachmentOptimal,
                                      vk::ImageLayout::eTransferSrcOptimal,
                                      vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                      {},
                                      vk::PipelineStageFlagBits::eTransfer,
                                      vk::AccessFlagBits::eTransferRead);

                    device.Submit(cmd);
                }

                for (size_t i = 0; i < xrViews.size(); i++) {
                    xrSystem->SubmitView(xrViews[i].second.eye, xrRenderTargets[i].color.get());
                }
            }
        #endif

            renderer->EndFrame();
        }

        context->SwapBuffers();

        #ifdef SP_XR_SUPPORT
        if (xrSystem) xrSystem->WaitFrame();
        #endif

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
