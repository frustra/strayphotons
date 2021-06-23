#pragma once

#include "core/CVar.hh"
#include "core/Common.hh"
#include "ecs/components/View.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/opengl/Graphics.hh"
#include "graphics/opengl/PerfTimer.hh"
#include "graphics/opengl/RenderTarget.hh"

#include <string>

struct GLFWwindow;

namespace sp {
    class Device;
    class ShaderSet;
    class RenderTarget;

    extern CVar<glm::ivec2> CVarWindowSize;
    extern CVar<float> CVarWindowScale;
    extern CVar<float> CVarFieldOfView;
    extern CVar<int> CVarWindowFullscreen;

    class GlfwGraphicsContext : public GraphicsContext {
    public:
        GlfwGraphicsContext();
        virtual ~GlfwGraphicsContext();

        // Potential GraphicsContext function implementations
        void Init() override;
        bool ShouldClose() override;
        void BeginFrame() override;
        void SwapBuffers() override;
        void PopulatePancakeView(ecs::View &view) override;
        void PrepareForView(ecs::View &view) override;
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

        shared_ptr<GpuTexture> LoadTexture(shared_ptr<Image> image, bool genMipmap = true) override;

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
    };
} // namespace sp
