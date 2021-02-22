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

        void SwapBuffers();

    private:
        glm::ivec2 prevWindowSize, prevWindowPos;
        int prevFullscreen = 0;
        double windowScale = 1.0;
        vector<glm::ivec2> monitorModes;

    protected:
        GLFWwindow *window = nullptr;
        Game *game;
        InputManager *input = nullptr;
        std::unique_ptr<GlfwActionSource> glfwActionSource = nullptr;
    };
} // namespace sp
