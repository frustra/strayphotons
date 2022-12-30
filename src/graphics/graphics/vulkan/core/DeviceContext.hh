#pragma once

#include "assets/Async.hh"
#include "core/DispatchQueue.hh"
#include "core/Hashing.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/vulkan/core/BufferPool.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/HandlePool.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/RenderPass.hh"

#include <atomic>
#include <future>
#include <robin_hood.h>
#include <variant>

struct GLFWwindow;

namespace tracy {
    class VkCtx;
}

namespace ecs {
    struct View;
}

namespace sp {
    class CFuncCollection;
}

namespace sp::vulkan {
    const uint32 MAX_FRAMES_IN_FLIGHT = 2;

    class DescriptorPool;
    class PerfTimer;
    class Pipeline;
    class PipelineManager;
    struct PipelineCompileInput;
    class Shader;

    class DeviceContext final : public sp::GraphicsContext {
    public:
        DeviceContext(bool enableValidationLayers = false, bool enableSwapchain = true);
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
        void UpdateInputModeFromFocus() override;

        // These functions are acceptable in the base GraphicsContext class,
        // but really shouldn't needed. They should be replaced with a generic "Settings" API
        // that allows modules to populate a Settings / Options menu entry
        const std::vector<glm::ivec2> &MonitorModes() override;
        const glm::ivec2 CurrentMode() override;

        void PrepareWindowView(ecs::View &view) override;

        // Returns a CommandContext that can be recorded and submitted within the current frame.
        // The each frame's CommandPool will be reset at the beginning of the frame.
        CommandContextPtr GetFrameCommandContext(CommandContextType type = CommandContextType::General);

        // Returns a CommandContext that can be recorded on any thread, and isn't reset until its fence is signalled.
        CommandContextPtr GetFencedCommandContext(CommandContextType type = CommandContextType::General);

        // Releases *cmd back to the DeviceContext and resets cmd
        void Submit(CommandContextPtr &cmd,
            vk::ArrayProxy<const vk::Semaphore> signalSemaphores = {},
            vk::ArrayProxy<const vk::Semaphore> waitSemaphores = {},
            vk::ArrayProxy<const vk::PipelineStageFlags> waitStages = {},
            vk::Fence fence = {},
            bool lastSubmit = false);

        void Submit(vk::ArrayProxyNoTemporaries<CommandContextPtr> cmds,
            vk::ArrayProxy<const vk::Semaphore> signalSemaphores = {},
            vk::ArrayProxy<const vk::Semaphore> waitSemaphores = {},
            vk::ArrayProxy<const vk::PipelineStageFlags> waitStages = {},
            vk::Fence fence = {},
            bool lastSubmit = false);

        BufferPtr AllocateBuffer(BufferLayout layout, vk::BufferUsageFlags usage, VmaMemoryUsage residency);
        BufferPtr AllocateBuffer(vk::BufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo);

        template<typename T>
        BufferPtr CreateBuffer(const T *srcData,
            size_t srcCount,
            vk::BufferUsageFlags usage,
            VmaMemoryUsage residency) {
            auto buf = AllocateBuffer(sizeof(T) * srcCount, usage, residency);
            buf->CopyFrom(srcData, srcCount);
            return buf;
        }

        AsyncPtr<Buffer> CreateBuffer(const InitialData &data,
            vk::BufferCreateInfo bufferInfo,
            VmaAllocationCreateInfo allocInfo);

        AsyncPtr<Buffer> CreateBuffer(const InitialData &data, vk::BufferUsageFlags usage, VmaMemoryUsage residency);

        BufferPtr GetBuffer(const BufferDesc &desc);

        struct BufferTransfer {
            BufferTransfer() {}

            template<typename Src, typename Dst>
            BufferTransfer(const Src &src, const Dst &dst) : src(src), dst(dst) {}

            std::variant<BufferPtr, SubBufferPtr> src;
            std::variant<BufferPtr, SubBufferPtr> dst;
        };
        AsyncPtr<void> TransferBuffers(vk::ArrayProxy<const BufferTransfer> batch);

        ImagePtr AllocateImage(vk::ImageCreateInfo info,
            VmaMemoryUsage residency,
            vk::ImageUsageFlags declaredUsage = {});
        AsyncPtr<Image> CreateImage(ImageCreateInfo createInfo, const InitialData &data = {});
        ImageViewPtr CreateImageView(ImageViewCreateInfo info);
        AsyncPtr<ImageView> CreateImageAndView(const ImageCreateInfo &imageInfo,
            const ImageViewCreateInfo &viewInfo, // image field is filled in automatically
            const InitialData &data = {});
        ImageViewPtr SwapchainImageView();
        vk::Sampler GetSampler(SamplerType type);
        vk::Sampler GetSampler(const vk::SamplerCreateInfo &info);

        AsyncPtr<ImageView> LoadAssetImage(shared_ptr<const sp::Image> image, bool genMipmap = false, bool srgb = true);
        shared_ptr<GpuTexture> LoadTexture(shared_ptr<const sp::Image> image, bool genMipmap = true) override;

        ShaderHandle LoadShader(string_view name);
        shared_ptr<Shader> GetShader(ShaderHandle handle) const;

        // Returns a Pipeline from cache, keyed by `input`. Builds the pipeline if not found in cache.
        shared_ptr<Pipeline> GetPipeline(const PipelineCompileInput &input);

        // Returns a RenderPass from cache, keyed by `info.state`. Builds the render pass if not found in cache.
        shared_ptr<RenderPass> GetRenderPass(const RenderPassInfo &info);

        // Returns a Framebuffer from cache, keyed by `info.state`. Builds the render pass if not found in cache.
        shared_ptr<Framebuffer> GetFramebuffer(const RenderPassInfo &info);

        // Returns a descriptor set, in which binding 0 is a variable sized array of sampler/image descriptors.
        // Bindless descriptor sets stay allocated until the DeviceContext shuts down.
        vk::DescriptorSet CreateBindlessDescriptorSet();

        SharedHandle<vk::Fence> GetEmptyFence();
        SharedHandle<vk::Semaphore> GetEmptySemaphore(vk::Fence inUseUntilFence);

        using TemporaryObject = std::variant<BufferPtr, ImageViewPtr, SharedHandle<vk::Semaphore>>;
        void PushInFlightObject(TemporaryObject object, vk::Fence fence);

        const vk::PhysicalDeviceLimits &Limits() const {
            return physicalDeviceProperties.properties.limits;
        }

        const vk::PhysicalDeviceDescriptorIndexingProperties &IndexingLimits() const {
            return physicalDeviceDescriptorIndexingProperties;
        }

        vk::FormatProperties FormatProperties(vk::Format format) const;

        vk::Format SelectSupportedFormat(vk::FormatProperties requiredProps,
            vk::ArrayProxy<const vk::Format> possibleFormats);

        vk::Format SelectSupportedFormat(vk::FormatFeatureFlags optimalTilingFeatures,
            vk::ArrayProxy<const vk::Format> possibleFormats) {
            return SelectSupportedFormat({{}, optimalTilingFeatures, {}}, possibleFormats);
        }

#ifdef TRACY_ENABLE
        tracy::VkCtx *GetTracyContext(CommandContextType type);
#endif

        uint32 QueueFamilyIndex(CommandContextType type) {
            return queueFamilyIndex[QueueType(type)];
        }

        GLFWwindow *GetWindow() {
            return window;
        }

        void *Win32WindowHandle() override;

        VkDebugUtilsMessageTypeFlagsEXT disabledDebugMessages = 0;

        static void DeleteAllocator(VmaAllocator allocator);

        void FlushMainQueue() {
            frameEndQueue.Flush(true);
        }

        template<typename CallbackFn>
        void ExecuteAfterFence(vk::Fence fence, CallbackFn &&callback) {
            frameBeginQueue.Dispatch<void>([this, callback, fence]() {
                if (device->getFenceStatus(fence) == vk::Result::eSuccess) {
                    callback();
                } else {
                    ExecuteAfterFence(fence, callback);
                }
            });
        }

        template<typename CallbackFn>
        void ExecuteAfterFrameFence(CallbackFn &&callback) {
            ExecuteAfterFence(*Frame().inFlightFence, std::forward<CallbackFn>(callback));
        }

        PerfTimer *GetPerfTimer() const {
            return perfTimer.get();
        }

        uint32_t GetMeasuredFPS() const override {
            return measuredFrameRate.load();
        }

        void SetTitle(std::string title) override;

    private:
        void CreateSwapchain();
        void CreateTestPipeline();
        void RecreateSwapchain();

        void PrepareResourcesForFrame();

        shared_ptr<Shader> CreateShader(const string &name, Hash64 compareHash);

        std::thread::id mainThread;
        std::thread::id renderThread;
        vk::UniqueInstance instance;
        vk::UniqueDebugUtilsMessengerEXT debugMessenger;
        vk::UniqueSurfaceKHR surface;
        vk::PhysicalDevice physicalDevice;
        vk::PhysicalDeviceProperties2 physicalDeviceProperties;
        vk::PhysicalDeviceDescriptorIndexingProperties physicalDeviceDescriptorIndexingProperties;
        vk::UniqueDevice device;

        unique_ptr<VmaAllocator_T, void (*)(VmaAllocator)> allocator;
        unique_ptr<PerfTimer> perfTimer;

#ifdef TRACY_ENABLE
        struct {
            vector<vk::UniqueCommandPool> cmdPools;
            vector<vk::UniqueCommandBuffer> cmdBuffers;
            vector<tracy::VkCtx *> tracyContexts;
        } tracing;
#endif

        unique_ptr<HandlePool<vk::Fence>> fencePool;
        unique_ptr<HandlePool<vk::Semaphore>> semaphorePool;
        unique_ptr<PipelineManager> pipelinePool;
        unique_ptr<RenderPassManager> renderPassPool;
        unique_ptr<FramebufferManager> framebufferPool;

        shared_ptr<DescriptorPool> bindlessImageSamplerDescriptorPool;

        std::array<vk::Queue, QUEUE_TYPES_COUNT> queues;
        std::array<uint32, QUEUE_TYPES_COUNT> queueFamilyIndex;
        std::array<uint32, QUEUE_TYPES_COUNT> queueLastSubmit;
        vk::Extent3D imageTransferGranularity;

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

        struct InFlightObject {
            TemporaryObject object;
            vk::Fence fence;
        };

        struct PooledBuffer {
            BufferPtr buffer;
            vk::DeviceSize size;
            bool used;
        };

        struct FrameContext {
            vk::UniqueSemaphore imageAvailableSemaphore, renderCompleteSemaphore;
            vk::UniqueFence inFlightFence;

            // Stores all command contexts created for this frame, so they can be reused in later frames
            std::array<CommandContextPool, QUEUE_TYPES_COUNT> commandContexts;

            vector<InFlightObject> inFlightObjects;
        };

        std::array<FrameContext, MAX_FRAMES_IN_FLIGHT> frameContexts;
        uint32 frameIndex = 0;

        FrameContext &Frame() {
            return frameContexts[frameIndex];
        }

        struct ThreadContext {
            std::array<vk::UniqueCommandPool, QUEUE_TYPES_COUNT> commandPools;
            std::array<unique_ptr<HandlePool<CommandContextPtr>>, QUEUE_TYPES_COUNT> commandContexts;
            std::array<vector<SharedHandle<CommandContextPtr>>, QUEUE_TYPES_COUNT> pendingCommandContexts;

            unique_ptr<BufferPool> bufferPool;
            std::atomic_bool printBufferStats;

            void ReleaseAvailableResources();
        };

        vector<unique_ptr<ThreadContext>> threadContexts;

        std::atomic_uint32_t nextThreadIndex = 0;

        ThreadContext &Thread() {
            thread_local size_t threadIndex = nextThreadIndex.fetch_add(1);
            Assert(threadIndex < threadContexts.size(), "ran out of thread contexts");
            return *threadContexts[threadIndex];
        }

        robin_hood::unordered_map<string, ShaderHandle, StringHash, StringEqual> shaderHandles;
        vector<shared_ptr<Shader>> shaders; // indexed by ShaderHandle minus 1
        std::atomic_bool reloadShaders;

        robin_hood::unordered_map<SamplerType, vk::UniqueSampler> namedSamplers;

        using SamplerKey = HashKey<VkSamplerCreateInfo>;
        robin_hood::unordered_map<SamplerKey, vk::UniqueSampler, SamplerKey::Hasher> adhocSamplers;

        glm::ivec2 glfwWindowSize;
        glm::ivec2 storedWindowPos; // Remember window location when returning from fullscreen
        int glfwFullscreen = 0;
        std::vector<glm::ivec2> monitorModes;
        double lastFrameEnd = 0, fpsTimer = 0;
        uint32 frameCounter = 0, frameCounterThisSecond = 0;
        std::atomic_uint32_t measuredFrameRate;
        GLFWwindow *window = nullptr;

        DispatchQueue frameBeginQueue, frameEndQueue, allocatorQueue;

        unique_ptr<CFuncCollection> funcs;
    };
} // namespace sp::vulkan
