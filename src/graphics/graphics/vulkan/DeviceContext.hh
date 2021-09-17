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
        DeviceContext(bool enableValidationLayers = false);
        virtual ~DeviceContext();

        // Access the underlying Vulkan device via the arrow operator
        vk::Device *operator->() {
            return &Device();
        }

        vk::Device &Device() {
            return *device;
        }

        vk::PhysicalDevice &PhysicalDevice() {
            return physicalDevice;
        }

        vk::Instance &Instance() {
            return *instance;
        }

        vk::Queue &GetQueue(CommandContextType type) {
            return queues[QueueType(type)];
        }

        // Potential GraphicsContext function implementations
        bool ShouldClose() override;
        void BeginFrame() override;
        void SwapBuffers() override;
        void EndFrame() override;
        void WaitIdle() override {
            device->waitIdle();
        }

        void UpdateInputModeFromFocus();

        // These functions are acceptable in the base GraphicsContext class,
        // but really shouldn't needed. They should be replaced with a generic "Settings" API
        // that allows modules to populate a Settings / Options menu entry
        const std::vector<glm::ivec2> &MonitorModes() override;
        const glm::ivec2 CurrentMode() override;

        void PrepareWindowView(ecs::View &view) override;

        CommandContextPtr GetCommandContext(CommandContextType type = CommandContextType::General);

        // Releases *cmd back to the DeviceContext and resets cmd
        void Submit(CommandContextPtr &cmd,
                    vk::ArrayProxy<const vk::Semaphore> signalSemaphores = {},
                    vk::ArrayProxy<const vk::Semaphore> waitSemaphores = {},
                    vk::ArrayProxy<const vk::PipelineStageFlags> waitStages = {},
                    vk::Fence fence = {});

        BufferPtr AllocateBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, VmaMemoryUsage residency);

        template<typename T>
        BufferPtr CreateBuffer(const T *srcData,
                               size_t srcCount,
                               vk::BufferUsageFlags usage,
                               VmaMemoryUsage residency) {
            auto buf = AllocateBuffer(sizeof(T) * srcCount, usage, residency);
            buf->CopyFrom(srcData, srcCount);
            return buf;
        }

        ImagePtr AllocateImage(const vk::ImageCreateInfo &info, VmaMemoryUsage residency);
        ImagePtr CreateImage(vk::ImageCreateInfo createInfo,
                             const uint8 *srcData = nullptr,
                             size_t srcDataSize = 0,
                             bool genMipmap = false);
        ImageViewPtr CreateImageView(ImageViewCreateInfo info);
        ImageViewPtr CreateImageAndView(const vk::ImageCreateInfo &imageInfo,
                                        ImageViewCreateInfo viewInfo, // image field is filled in automatically
                                        const uint8 *srcData = nullptr,
                                        size_t srcDataSize = 0,
                                        bool genMipmap = false);
        vk::Sampler GetSampler(SamplerType type);
        vk::Sampler GetSampler(const vk::SamplerCreateInfo &info);

        shared_ptr<GpuTexture> LoadTexture(shared_ptr<const sp::Image> image, bool genMipmap = true) override;

        RenderPassInfo SwapchainRenderPassInfo(bool depth = false, bool stencil = false);

        ShaderHandle LoadShader(const string &name);
        shared_ptr<Shader> GetShader(ShaderHandle handle) const;
        void ReloadShaders();

        shared_ptr<Pipeline> GetGraphicsPipeline(const PipelineCompileInput &input);
        shared_ptr<RenderPass> GetRenderPass(const RenderPassInfo &info);
        shared_ptr<Framebuffer> GetFramebuffer(const RenderPassInfo &info);

        vk::Semaphore GetEmptySemaphore();

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

        uint32 QueueFamilyIndex(CommandContextType type) {
            return queueFamilyIndex[QueueType(type)];
        }

        GLFWwindow *GetWindow() {
            return window;
        }

        void *Win32WindowHandle() override;

    private:
        void SetTitle(string title);

        void CreateSwapchain();
        void CreateTestPipeline();
        void RecreateSwapchain();

        void PrepareResourcesForFrame();

        shared_ptr<Shader> CreateShader(const string &name, Hash64 compareHash);

        vk::UniqueInstance instance;
        vk::UniqueDebugUtilsMessengerEXT debugMessenger;
        vk::UniqueSurfaceKHR surface;
        vk::PhysicalDevice physicalDevice;
        vk::PhysicalDeviceProperties physicalDeviceProperties;
        vk::UniqueDevice device;

        vector<vk::UniqueSemaphore> semaphores;

        unique_ptr<PipelineManager> pipelinePool;
        unique_ptr<RenderPassManager> renderPassPool;
        unique_ptr<FramebufferManager> framebufferPool;

        VmaAllocator allocator = VK_NULL_HANDLE;

        std::array<vk::Queue, QUEUE_TYPES_COUNT> queues;
        std::array<uint32, QUEUE_TYPES_COUNT> queueFamilyIndex;
        vk::Extent3D imageTransferGranularity;

        uint32 swapchainVersion = 0;
        vk::UniqueSwapchainKHR swapchain;
        vk::Extent2D swapchainExtent;

        struct SwapchainImageContext {
            vk::Fence inFlightFence; // points at a fence owned by FrameContext
            ImageViewPtr imageView;
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

        struct InFlightBuffer {
            vk::UniqueFence fence;
            BufferPtr buffer;
        };

        struct FrameContext {
            vk::UniqueSemaphore imageAvailableSemaphore, renderCompleteSemaphore;
            vk::UniqueFence inFlightFence;

            // Stores all command contexts created for this frame, so they can be reused in later frames
            // TODO: multiple threads need their own pools
            std::array<CommandContextPool, QUEUE_TYPES_COUNT> commandContexts;

            vector<InFlightBuffer> inFlightBuffers;
        };

        std::array<FrameContext, MAX_FRAMES_IN_FLIGHT> frameContexts;
        uint32 frameIndex = 0;

        FrameContext &Frame() {
            return frameContexts[frameIndex];
        }

        ImageViewPtr depthImageView; // TODO: move to render target pool

        robin_hood::unordered_map<string, ShaderHandle> shaderHandles;
        vector<shared_ptr<Shader>> shaders; // indexed by ShaderHandle minus 1

        robin_hood::unordered_map<SamplerType, vk::UniqueSampler> namedSamplers;

        using SamplerKey = HashKey<VkSamplerCreateInfo>;
        robin_hood::unordered_map<SamplerKey, vk::UniqueSampler, SamplerKey::Hasher> adhocSamplers;

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
