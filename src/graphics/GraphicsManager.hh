#pragma once

#ifdef SP_GRAPHICS_SUPPORT

    #include "core/Common.hh"
    #include "ecs/Ecs.hh"

    #ifdef SP_GRAPHICS_SUPPORT_GL
        #include "graphics/opengl/GLRenderTarget.hh"
        #include "graphics/opengl/PerfTimer.hh"
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_VK
        #include "graphics/vulkan/core/Common.hh"
        #include "graphics/vulkan/core/PerfTimer.hh"
    #endif

    #ifdef SP_INPUT_SUPPORT_GLFW
        #include "input/glfw/GlfwInputHandler.hh"
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
        class ProfilerGui;
    } // namespace vulkan
    #endif

    class GraphicsManager {
    public:
        GraphicsManager(Game *game);
        ~GraphicsManager();

        void Init();
        bool HasActiveContext();

        bool Frame();

        GraphicsContext *GetContext();

    private:
        Game *game;
        unique_ptr<GraphicsContext> context;
        ecs::NamedEntity flatviewEntity;

    #ifdef SP_GRAPHICS_SUPPORT_VK
        unique_ptr<vulkan::Renderer> renderer;
        vulkan::PerfTimer timer;
    #endif

    #ifdef SP_GRAPHICS_SUPPORT_GL
        unique_ptr<VoxelRenderer> renderer;
        shared_ptr<ProfilerGui> profilerGui;
        PerfTimer timer;

        #ifdef SP_XR_SUPPORT
        std::vector<shared_ptr<GLRenderTarget>> xrRenderTargets;
        std::vector<glm::mat4> xrRenderPoses;
        #endif
    #endif

    #ifdef SP_INPUT_SUPPORT_GLFW
        unique_ptr<GlfwInputHandler> glfwInputHandler;
    #endif
    };
} // namespace sp

#endif
