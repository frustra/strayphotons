#pragma once

#include "ecs/components/View.hh"
#include "graphics/core/GraphicsContext.hh"

#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace sp {
    class VulkanGraphicsContext final : public GraphicsContext {
    public:
        VulkanGraphicsContext();
        virtual ~VulkanGraphicsContext();

        // Potential GraphicsContext function implementations
        bool ShouldClose() override;
        void BeginFrame() override;
        void SwapBuffers() override;
        void EndFrame() override;

        void DisableCursor() override;
        void EnableCursor() override;

        // These functions are acceptable in the base GraphicsContext class,
        // but really shouldn't needed. They should be replaced with a generic "Settings" API
        // that allows modules to populate a Settings / Options menu entry
        const std::vector<glm::ivec2> &MonitorModes() override;
        const glm::ivec2 CurrentMode() override;

        std::shared_ptr<GpuTexture> LoadTexture(shared_ptr<Image> image, bool genMipmap = true) override;

        void PrepareWindowView(ecs::View &view) override;

        GLFWwindow *GetWindow() {
            return window;
        }

    private:
        void SetTitle(string title);

        vk::UniqueInstance instance;
        vk::UniqueDebugUtilsMessengerEXT debugMessenger;
        vk::UniqueSurfaceKHR surface;
        vk::UniqueDevice device;
        vk::Queue graphicsQueue;
        vk::Queue presentQueue;

        vk::UniqueSwapchainKHR swapChain;
        vector<vk::Image> swapChainImages;
        vk::Format swapChainImageFormat;
        vk::Extent2D swapChainExtent;
        vector<vk::UniqueImageView> swapChainImageViews;
        vk::UniqueCommandPool commandPool;
        vector<vk::CommandBuffer> commandBuffers;

        // test pipeline
        vk::UniqueRenderPass renderPass;
        vk::UniquePipelineLayout pipelineLayout;
        vk::UniquePipeline graphicsPipeline;
        vector<vk::UniqueFramebuffer> swapChainFramebuffers;

        vk::UniqueSemaphore imageAvailableSem, renderCompleteSem;

        uint32_t imageIndex; // index of the swapchain currently being rendered

        glm::ivec2 glfwWindowSize;
        glm::ivec2 storedWindowPos; // Remember window location when returning from fullscreen
        int glfwFullscreen = 0;
        std::vector<glm::ivec2> monitorModes;
        double lastFrameEnd = 0, fpsTimer = 0;
        int frameCounter = 0;
        GLFWwindow *window = nullptr;
    };
} // namespace sp
