#pragma once

#include "core/Common.hh"
#include "ecs/components/View.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/opengl/Graphics.hh"
#include "graphics/opengl/RenderTargetPool.hh"

#include <string>

struct GLFWwindow;

namespace sp {
    class Device;

    class GlfwGraphicsContext : public GraphicsContext {
    public:
        GlfwGraphicsContext();
        virtual ~GlfwGraphicsContext();

        // Potential GraphicsContext function implementations
        void Init() override;
        bool ShouldClose() override;
        void BeginFrame() override;
        void SwapBuffers() override;
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

        std::shared_ptr<GLRenderTarget> GetRenderTarget(const RenderTargetDesc &desc) {
            return rtPool.Get(desc);
        }

        GLuint GetFramebuffer(uint32 numAttachments,
                              GLRenderTarget **attachments,
                              GLRenderTarget *depthStencilAttachment) {
            return rtPool.GetFramebuffer(numAttachments, attachments, depthStencilAttachment);
        }

        std::shared_ptr<GpuTexture> LoadTexture(shared_ptr<Image> image, bool genMipmap = true) override;

    private:
        void SetTitle(string title);
        void CreateGlfwWindow(glm::ivec2 initialSize = {640, 480});
        void PrepareWindowView(ecs::View &frameBufferView);

        RenderTargetPool rtPool;

        glm::ivec2 glfwWindowSize;
        glm::ivec2 storedWindowPos; // Remember window location when returning from fullscreen
        int glfwFullscreen = 0;
        vector<glm::ivec2> monitorModes;
        double lastFrameEnd = 0, fpsTimer = 0;
        int frameCounter = 0;
        GLFWwindow *window = nullptr;
    };
} // namespace sp
