#pragma once

#ifdef SP_GRAPHICS_SUPPORT

    #include "core/Common.hh"
    #include "ecs/Ecs.hh"

    #ifdef SP_GRAPHICS_SUPPORT_GL
        #include "graphics/opengl/GLRenderTarget.hh"
        #include "graphics/opengl/PerfTimer.hh"
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        #include "graphics/vulkan/Common.hh"
    #endif

    #include <glm/glm.hpp>
    #include <memory>
    #include <vector>

namespace sp {
    class Game;
    class GraphicsContext;
    class ProfilerGui;

    #ifdef SP_GRAPHICS_SUPPORT_GL
    class VoxelRenderer;
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
    namespace vulkan {
        class Renderer;
        class GuiRenderer;
        class ImageView;
    } // namespace vulkan
    #endif

    class GraphicsManager {
    public:
        GraphicsManager(Game *game);
        ~GraphicsManager();

        void Init();
        bool HasActiveContext();

        void RenderLoading();

        bool Frame();

        GraphicsContext *GetContext();

    private:
        Game *game;
        unique_ptr<GraphicsContext> context;

    #ifdef SP_GRAPHICS_SUPPORT_GL
        unique_ptr<VoxelRenderer> renderer;
        shared_ptr<ProfilerGui> profilerGui;
        PerfTimer timer;
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        unique_ptr<vulkan::Renderer> renderer;
        unique_ptr<vulkan::GuiRenderer> debugGuiRenderer;

        std::vector<vulkan::BufferPtr> viewStateUniformBuffers;
    #endif

    #ifdef SP_XR_SUPPORT
        #ifdef SP_GRAPHICS_SUPPORT_GL
        std::vector<shared_ptr<GLRenderTarget>> xrRenderTargets;
        #endif

        #ifdef SP_GRAPHICS_SUPPORT_VK
        shared_ptr<vulkan::ImageView> xrColor, xrDepth;
        #endif
    #endif
    };
} // namespace sp

#endif
