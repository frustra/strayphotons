#pragma once

#include "Common.hh"
#include "ecs/components/View.hh"
#include "graphics/Graphics.hh"
#include "graphics/GraphicsContext.hh"
#include "graphics/RenderTarget.hh"

#include <core/PerfTimer.hh>
#include <game/input/GlfwActionSource.hh>
#include <string>

struct GLFWwindow;

namespace sp {
    class Device;
    class ShaderSet;
    class Game;
    class InputManager;
    class RenderTarget;

    class GlfwGraphicsContext : public GraphicsContext {
    public:
        GlfwGraphicsContext(Game *game);
        virtual ~GlfwGraphicsContext();

        // Potential GraphicsContext function implementations
        void Init() override;
        bool ShouldClose() override;
        void BeginFrame() override;
        void SwapBuffers() override;
        void PopulatePancakeView(ecs::View& view) override;
        void PrepareForView(ecs::View& view) override;
        void EndFrame() override;

        // These functions are acceptable in the base GraphicsContext class, 
        // but really shouldn't needed. They should be replaced with a generic "Settings" API
        // that allows modules to populate a Settings / Options menu entry
        const vector<glm::ivec2> &MonitorModes() override;
        const glm::ivec2 CurrentMode() override;

        // Specific to GlfwGraphicsContext
        GLFWwindow *GetWindow() {
            return window;
        }

        void DisableCursor();
        void EnableCursor();

    private:
        void SetTitle(string title);
        void CreateWindow(glm::ivec2 initialSize = {640, 480});
        void ResizeWindow(ecs::View &frameBufferView, double scale, int fullscreen);

    private:
        glm::ivec2 prevWindowSize, prevWindowPos;
        int prevFullscreen = 0;
        double windowScale = 1.0;
        vector<glm::ivec2> monitorModes;
        double lastFrameEnd = 0, fpsTimer = 0;
        int frameCounter = 0;
        GLFWwindow *window = nullptr;
        InputManager *input = nullptr;
        std::unique_ptr<GlfwActionSource> glfwActionSource = nullptr;
    };
} // namespace sp
