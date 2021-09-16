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

                std::vector<RenderTarget *> xrRenderTargets(xrViews.size());

                for (size_t i = 0; i < xrViews.size(); i++) {
                    RenderPhase xrSubPhase("XrView", timer);

                    xrRenderTargets[i] = xrSystem->GetRenderTarget(xrViews[i].second.eye);

                    renderer->RenderPass(xrViews[i].first, lock, xrRenderTargets[i]);
                }

                for (size_t i = 0; i < xrViews.size(); i++) {
                    RenderPhase xrSubPhase("XrViewSubmit", timer);

                    xrSystem->SubmitView(xrViews[i].second.eye, xrRenderTargets[i]->GetTexture());
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
                for (size_t i = 0; i < xrViews.size(); i++) {
                    auto &view = xrViews[i].first;

                    if (!vulkanViews[i].color || vulkanViews[i].color->GetWidth() != view.extents.x ||
                        vulkanViews[i].color->GetHeight() != view.extents.y) {

                        vk::ImageCreateInfo imageInfo;
                        imageInfo.imageType = vk::ImageType::e2D;
                        imageInfo.extent = vk::Extent3D(view.extents.x, view.extents.y, 1);

                        imageInfo.format = vk::Format::eR8G8B8A8Srgb;
                        imageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled |
                                          vk::ImageUsageFlagBits::eTransferSrc;
                        vulkanViews[i].color = device.CreateImageAndView(imageInfo, {});

                        imageInfo.format = vk::Format::eD24UnormS8Uint;
                        imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
                        vulkanViews[i].depth = device.CreateImageAndView(imageInfo, {});
                    }

                    cmd = device.GetCommandContext();
                    vk::ImageMemoryBarrier imageMemoryBarrier;
                    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead |
                                                       vk::AccessFlagBits::eTransferRead;
                    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
                    imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
                    imageMemoryBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
                    imageMemoryBarrier.image = *vulkanViews[i].color->Image();
                    imageMemoryBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
                    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
                    imageMemoryBarrier.subresourceRange.levelCount = 1;
                    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
                    imageMemoryBarrier.subresourceRange.layerCount = 1;
                    imageMemoryBarrier.srcQueueFamilyIndex = device.QueueFamilyIndex(
                        vulkan::CommandContextType::General);
                    imageMemoryBarrier.dstQueueFamilyIndex = device.QueueFamilyIndex(
                        vulkan::CommandContextType::General);
                    cmd->Raw().pipelineBarrier(
                        vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eFragmentShader,
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        {},
                        {},
                        {},
                        {imageMemoryBarrier});

                    imageMemoryBarrier.image = *vulkanViews[i].depth->Image();
                    imageMemoryBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth |
                                                                     vk::ImageAspectFlagBits::eStencil;
                    imageMemoryBarrier.srcAccessMask = {};
                    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                    imageMemoryBarrier.newLayout = vk::ImageLayout::eDepthAttachmentOptimal;
                    cmd->Raw().pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe,
                                               vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                               {},
                                               {},
                                               {},
                                               {imageMemoryBarrier});

                    std::array<float, 4> clearColor = {0.0f, 1.0f, 0.0f, 1.0f};

                    vulkan::RenderPassInfo info;
                    info.PushColorAttachment(vulkanViews[i].color,
                                             vulkan::LoadOp::Clear,
                                             vulkan::StoreOp::Store,
                                             clearColor);
                    info.SetDepthStencilAttachment(vulkanViews[i].depth,
                                                   vulkan::LoadOp::Clear,
                                                   vulkan::StoreOp::DontCare);

                    cmd->BeginRenderPass(info);

                    renderer->RenderPass(cmd, view, lock);

                    cmd->EndRenderPass();

                    imageMemoryBarrier.image = *vulkanViews[i].color->Image();
                    imageMemoryBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
                    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
                    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
                    imageMemoryBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
                    imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
                    cmd->Raw().pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
                                               vk::PipelineStageFlagBits::eTransfer,
                                               {},
                                               {},
                                               {},
                                               {imageMemoryBarrier});

                    device.Submit(cmd);
                }

                vr::VRTextureBounds_t bounds;
                bounds.uMin = 0.0f;
                bounds.uMax = 1.0f;
                bounds.vMin = 0.0f;
                bounds.vMax = 1.0f;

                vr::VRVulkanTextureData_t vulkanData;
                vulkanData.m_pDevice = device.Device();
                vulkanData.m_pPhysicalDevice = device.PhysicalDevice();
                vulkanData.m_pInstance = device.Instance();
                vulkanData.m_pQueue = device.GetQueue(vulkan::CommandContextType::General);
                vulkanData.m_nQueueFamilyIndex = device.QueueFamilyIndex(vulkan::CommandContextType::General);

                vulkanData.m_nFormat = (uint32)vk::Format::eR8G8B8A8Srgb;
                vulkanData.m_nSampleCount = 1;

                vr::Texture_t texture = {&vulkanData, vr::TextureType_Vulkan, vr::ColorSpace_Auto};

                for (size_t i = 0; i < xrViews.size(); i++) {
                    auto &view = xrViews[i].first;
                    vulkanData.m_nWidth = view.extents.x;
                    vulkanData.m_nHeight = view.extents.y;

                    vulkanData.m_nImage = (uint64_t)(VkImage)(**vulkanViews[i].color->Image());
                    vr::VRCompositor()->Submit((vr::EVREye)xrViews[i].second.eye, &texture, &bounds);
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
