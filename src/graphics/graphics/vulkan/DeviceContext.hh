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
    const int MAX_FRAMES_IN_FLIGHT = 2;

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

        shared_ptr<GpuTexture> LoadTexture(shared_ptr<const Image> image, bool genMipmap = true) override;

        void PrepareWindowView(ecs::View &view) override;

        CommandContextPtr GetCommandContext(CommandContextType type = CommandContextType::General);
        void Submit(CommandContextPtr &cmd); // releases *cmd back to the DeviceContext and resets cmd

        UniqueBuffer AllocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage residency);
        UniqueImage AllocateImage(const vk::ImageCreateInfo &info, VmaMemoryUsage residency);

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

        void *Win32WindowHandle() override;

        uint32 SwapchainVersion() {
            // incremented when the swapchain changes, any dependent pipelines need to be recreated
            return swapchainVersion;
        }

        UniqueID NextUniqueID() {
            return ++lastUniqueID;
        }

        const vk::PhysicalDeviceLimits &Limits() const {
            return physicalDeviceProperties.limits;
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
        vk::PhysicalDeviceProperties physicalDeviceProperties;
        vk::UniqueDevice device;

        unique_ptr<PipelineManager> pipelinePool;
        unique_ptr<RenderPassManager> renderPassPool;
        unique_ptr<FramebufferManager> framebufferPool;

        VmaAllocator allocator = VK_NULL_HANDLE;

        std::array<vk::Queue, QUEUE_TYPES_COUNT> queues;
        std::array<uint32, QUEUE_TYPES_COUNT> queueFamilyIndex;

        uint32 swapchainVersion = 0;
        vk::UniqueSwapchainKHR swapchain;
        vk::Extent2D swapchainExtent;

        struct SwapchainImageContext {
            vk::Fence inFlightFence; // points at a fence owned by FrameContext
            vk::Image image;

            // TODO: store custom image abstraction
            vk::ImageViewCreateInfo imageViewInfo;
            vk::UniqueImageView imageView;
        };

        vector<SwapchainImageContext> swapchainImageContexts;
        uint32 swapchainImageIndex;

        SwapchainImageContext &SwapchainImage() {
            return swapchainImageContexts[swapchainImageIndex];
        }

        struct CommandContextPool {
            vk::UniqueCommandPool commandPool;
            vector<CommandContextPtr> list;
            size_t nextIndex = 0;
        };

        struct FrameContext {
            vk::UniqueSemaphore imageAvailableSemaphore, renderCompleteSemaphore;
            vk::UniqueFence inFlightFence;

            // Stores all command contexts created for this frame, so they can be reused in later frames
            // TODO: multiple threads need their own pools
            std::array<CommandContextPool, QUEUE_TYPES_COUNT> commandContexts;

            void BeginFrame();
        };

        std::array<FrameContext, MAX_FRAMES_IN_FLIGHT> frameContexts;
        uint32 frameIndex = 0;

        FrameContext &Frame() {
            return frameContexts[frameIndex];
        }

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
