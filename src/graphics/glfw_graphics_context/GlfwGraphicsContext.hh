#pragma once

#include "Common.hh"
#include "ecs/components/View.hh"
#include "graphics/Graphics.hh"
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

    class GlfwGraphicsContext {
    public:
        GlfwGraphicsContext(Game *game);
        virtual ~GlfwGraphicsContext();

        // Potential GraphicsContext function implementations
        void Init();
        void BeginFrame();
        void SwapBuffers();
        void PopulatePancakeView(ecs::View& view);
        void PrepareForView(ecs::View& view);
        void EndFrame();

        // Specific to GlfwGraphicsContext
        void CreateWindow(glm::ivec2 initialSize = {640, 480});
        bool ShouldClose();
        void SetTitle(string title);
        void ResizeWindow(ecs::View &frameBufferView, double scale, int fullscreen);
        const vector<glm::ivec2> &MonitorModes();
        const glm::ivec2 CurrentMode();

        GLFWwindow *GetWindow() {
            return window;
        }

        void DisableCursor();
        void EnableCursor();

    private:
        glm::ivec2 prevWindowSize, prevWindowPos;
        int prevFullscreen = 0;
        double windowScale = 1.0;
        vector<glm::ivec2> monitorModes;
        double lastFrameEnd = 0, fpsTimer = 0;
        int frameCounter = 0;

    protected:
        GLFWwindow *window = nullptr;
        Game *game;
        InputManager *input = nullptr;
        std::unique_ptr<GlfwActionSource> glfwActionSource = nullptr;
    };
} // namespace sp
