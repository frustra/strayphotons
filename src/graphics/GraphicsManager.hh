#pragma once

#ifdef SP_GRAPHICS_SUPPORT

    #include "core/Common.hh"
    #include "core/RegisteredThread.hh"
    #include "ecs/Ecs.hh"
    #include "ecs/EntityRef.hh"

    #ifdef SP_GRAPHICS_SUPPORT_VK
        #include "graphics/vulkan/core/Common.hh"
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

    #ifdef SP_GRAPHICS_SUPPORT_VK
    namespace vulkan {
        class Renderer;
        class GuiRenderer;
        class ImageView;
        class ProfilerGui;
    } // namespace vulkan
    #endif

    class GraphicsManager : public RegisteredThread {
        LogOnExit logOnExit = "Graphics shut down ====================================================";

    public:
        GraphicsManager(Game *game, bool stepMode);
        ~GraphicsManager();

        void Init();
        void StartThread();
        void StopThread();
        bool HasActiveContext();
        bool InputFrame();

        GraphicsContext *GetContext();

    private:
        bool ThreadInit() override;
        void PreFrame() override;
        void PostFrame() override;
        void Frame() override;

        Game *game;
        bool stepMode;
        unique_ptr<GraphicsContext> context;
        ecs::EntityRef flatviewEntity;

        chrono_clock::time_point renderStart;

    #ifdef SP_GRAPHICS_SUPPORT_VK
        unique_ptr<vulkan::Renderer> renderer;
    #endif

    #ifdef SP_INPUT_SUPPORT_GLFW
        unique_ptr<GlfwInputHandler> glfwInputHandler;
    #endif
    };
} // namespace sp

#endif
