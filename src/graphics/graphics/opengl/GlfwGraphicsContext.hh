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

    class GlfwGraphicsContext final : public GraphicsContext {
    public:
        GlfwGraphicsContext();
        virtual ~GlfwGraphicsContext();

        // Potential GraphicsContext function implementations
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

        void *Win32WindowHandle() override;

        std::shared_ptr<GLRenderTarget> GetRenderTarget(const RenderTargetDesc &desc) {
            return rtPool.Get(desc);
        }

        GLuint GetFramebuffer(uint32_t numAttachments,
                              GLRenderTarget **attachments,
                              GLRenderTarget *depthStencilAttachment) {
            return rtPool.GetFramebuffer(numAttachments, attachments, depthStencilAttachment);
        }

        std::shared_ptr<GpuTexture> LoadTexture(std::shared_ptr<const Image> image, bool genMipmap = true) override;

        void PrepareWindowView(ecs::View &view) override;

    private:
        void SetTitle(string title);

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
