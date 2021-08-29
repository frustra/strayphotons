#pragma once

#include "Common.hh"
#include "Memory.hh"
#include "RenderPass.hh"
#include "UniqueID.hh"
#include "ecs/components/View.hh"
#include "graphics/core/GraphicsContext.hh"

#include <robin_hood.h>

struct GLFWwindow;

namespace sp {
    class CFuncCollection;
}

namespace sp::vulkan {
    class Pipeline;
    class PipelineManager;
    struct PipelineCompileInput;
    struct Shader;

    class DeviceContext final : public sp::GraphicsContext {
    public:
        DeviceContext();
        virtual ~DeviceContext();

        // Access the underlying Vulkan device via the arrow operator
        vk::Device *operator->() {
            return &Device();
        }

        vk::Device &Device() {
            return *device;
        }

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

        shared_ptr<GpuTexture> LoadTexture(shared_ptr<Image> image, bool genMipmap = true) override;

        void PrepareWindowView(ecs::View &view) override;

        vector<vk::UniqueCommandBuffer> AllocateCommandBuffers(
            uint32_t count,
            vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

        CommandContextPtr CreateCommandContext();

        UniqueBuffer AllocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage residency);
        UniqueImage AllocateImage(vk::ImageCreateInfo info, VmaMemoryUsage residency);

        RenderPassInfo SwapchainRenderPassInfo(bool depth = false, bool stencil = false);

        ShaderHandle LoadShader(const string &name);
        shared_ptr<Shader> GetShader(ShaderHandle handle) const;
        void ReloadShaders();

        shared_ptr<Pipeline> GetGraphicsPipeline(const PipelineCompileInput &input);
        shared_ptr<RenderPass> GetRenderPass(const RenderPassInfo &info);
        shared_ptr<Framebuffer> GetFramebuffer(const RenderPassInfo &info);

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

        vector<vk::ImageView> SwapchainImageViews() {
            vector<vk::ImageView> output;
            for (auto &imageView : swapchainImageViews) {
                output.push_back(*imageView);
            }
            return output;
        }

        UniqueID NextUniqueID() {
            return ++lastUniqueID;
        }

    private:
        void SetTitle(string title);

        void CreateSwapchain();
        void CreateTestPipeline();
        void RecreateSwapchain();

        shared_ptr<Shader> CreateShader(const string &name, Hash64 compareHash);

        vk::UniqueInstance instance;
        vk::UniqueDebugUtilsMessengerEXT debugMessenger;
        vk::UniqueSurfaceKHR surface;
        vk::PhysicalDevice physicalDevice;
        vk::UniqueDevice device;

        unique_ptr<PipelineManager> pipelinePool;
        unique_ptr<RenderPassManager> renderPassPool;
        unique_ptr<FramebufferManager> framebufferPool;

        VmaAllocator allocator = VK_NULL_HANDLE;

        uint32_t graphicsQueueFamily, presentQueueFamily;
        vk::Queue graphicsQueue, presentQueue;

        vk::UniqueCommandPool renderThreadCommandPool; // TODO: multiple threads need their own pools

        uint32_t swapchainVersion = 0;
        vk::UniqueSwapchainKHR swapchain;
        vector<vk::Image> swapchainImages;
        vk::Extent2D swapchainExtent;
        vector<vk::ImageViewCreateInfo> swapchainImageViewInfos;
        vector<vk::UniqueImageView> swapchainImageViews;

        std::vector<vk::UniqueSemaphore> imageAvailableSemaphores, renderCompleteSemaphores;
        std::vector<vk::UniqueFence> inFlightFences; // one per inflight frame
        std::vector<vk::Fence> imagesInFlight; // one per swapchain image

        size_t currentFrame = 0;
        uint32_t imageIndex; // index of the swapchain currently being rendered

        vk::ImageViewCreateInfo depthImageViewInfo;
        UniqueImage depthImage; // TODO: move to render target pool
        vk::UniqueImageView depthImageView;

        robin_hood::unordered_map<string, ShaderHandle> shaderHandles;
        vector<shared_ptr<Shader>> shaders; // indexed by ShaderHandle minus 1

        glm::ivec2 glfwWindowSize;
        glm::ivec2 storedWindowPos; // Remember window location when returning from fullscreen
        int glfwFullscreen = 0;
        std::vector<glm::ivec2> monitorModes;
        double lastFrameEnd = 0, fpsTimer = 0;
        uint32 frameCounter = 0, frameCounterThisSecond = 0;
        GLFWwindow *window = nullptr;

        unique_ptr<CFuncCollection> funcs;

        UniqueID lastUniqueID = 0;
    };
} // namespace sp::vulkan
