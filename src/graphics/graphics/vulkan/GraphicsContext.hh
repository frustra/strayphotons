#pragma once

#include "Memory.hh"
#include "ecs/components/View.hh"
#include "graphics/core/GraphicsContext.hh"

#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace sp::vulkan {
    class GraphicsContext final : public sp::GraphicsContext {
    public:
        GraphicsContext();
        virtual ~GraphicsContext();

        // Potential GraphicsContext function implementations
        bool ShouldClose() override;
        void BeginFrame() override;
        void SwapBuffers() override;
        void EndFrame() override;

        void UpdateInputModeFromFocus();

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

        vk::Fence CurrentFrameFence() {
            return *inFlightFences[currentFrame];
        }

        vk::Semaphore CurrentFrameImageAvailableSemaphore() {
            return *imageAvailableSemaphores[currentFrame];
        }

        vk::Semaphore CurrentFrameRenderCompleteSemaphore() {
            return *renderCompleteSemaphores[currentFrame];
        }

        vk::Device &Device() {
            return *device;
        }

        vk::Queue GraphicsQueue() {
            return graphicsQueue;
        }

        vk::Fence ResetCurrentFrameFence() {
            auto fence = CurrentFrameFence();
            device->resetFences({fence});
            return fence;
        }

        uint32_t CurrentSwapchainImageIndex() {
            return imageIndex;
        }

        uint32_t SwapchainVersion() {
            // incremented when the swapchain changes, any dependent pipelines need to be recreated
            return swapchainVersion;
        }

        vector<vk::UniqueCommandBuffer> CreateCommandBuffers(
            uint32_t count,
            vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary) {

            vk::CommandBufferAllocateInfo allocInfo;
            allocInfo.commandPool = *graphicsCommandPool;
            allocInfo.level = vk::CommandBufferLevel::ePrimary;
            allocInfo.commandBufferCount = count;

            return device->allocateCommandBuffersUnique(allocInfo);
        }

        vk::Format SwapchainImageFormat() {
            return swapchainImageFormat;
        }

        vector<vk::ImageView> SwapchainImageViews() {
            vector<vk::ImageView> output;
            for (auto &imageView : swapchainImageViews) {
                output.push_back(*imageView);
            }
            return output;
        }

        UniqueBuffer AllocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage residency);
        UniqueImage AllocateImage(vk::ImageCreateInfo info, VmaMemoryUsage residency);

    private:
        void SetTitle(string title);

        void CreateSwapchain();
        void CreateTestPipeline();
        void RecreateSwapchain();

        vk::UniqueInstance instance;
        vk::UniqueDebugUtilsMessengerEXT debugMessenger;
        vk::UniqueSurfaceKHR surface;
        vk::PhysicalDevice physicalDevice;
        vk::UniqueDevice device;

        VmaAllocator allocator = VK_NULL_HANDLE;

        uint32_t graphicsQueueFamily, presentQueueFamily;
        vk::Queue graphicsQueue, presentQueue;

        vk::UniqueCommandPool graphicsCommandPool;

        uint32_t swapchainVersion = 0;
        vk::UniqueSwapchainKHR swapchain;
        vector<vk::Image> swapchainImages;
        vk::Format swapchainImageFormat;
        vk::Extent2D swapchainExtent;
        vector<vk::UniqueImageView> swapchainImageViews;

        std::vector<vk::UniqueSemaphore> imageAvailableSemaphores, renderCompleteSemaphores;
        std::vector<vk::UniqueFence> inFlightFences; // one per inflight frame
        std::vector<vk::Fence> imagesInFlight; // one per swapchain image

        size_t currentFrame = 0;
        uint32_t imageIndex; // index of the swapchain currently being rendered

        glm::ivec2 glfwWindowSize;
        glm::ivec2 storedWindowPos; // Remember window location when returning from fullscreen
        int glfwFullscreen = 0;
        std::vector<glm::ivec2> monitorModes;
        double lastFrameEnd = 0, fpsTimer = 0;
        uint32 frameCounter = 0, frameCounterThisSecond = 0;
        GLFWwindow *window = nullptr;
    };
} // namespace sp::vulkan
