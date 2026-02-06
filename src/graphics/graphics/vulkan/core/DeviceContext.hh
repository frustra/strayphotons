/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Async.hh"
#include "common/Defer.hh"
#include "common/DispatchQueue.hh"
#include "common/Hashing.hh"
#include "graphics/core/GraphicsContext.hh"
#include "graphics/vulkan/Compositor.hh"
#include "graphics/vulkan/core/BufferPool.hh"
#include "graphics/vulkan/core/HandlePool.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/RenderPass.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/RenderGraph.hh"

#include <atomic>
#include <future>
#include <memory>
#include <robin_hood.h>
#include <variant>

namespace tracy {
    class VkCtx;
}

namespace ecs {
    struct View;
}

namespace sp {
    class CFuncCollection;
    class Game;
    class GraphicsManager;
    class GenericCompositor;
} // namespace sp

namespace sp::vulkan {
    const uint32 MAX_FRAMES_IN_FLIGHT = 2;

    class DescriptorPool;
    class PerfTimer;
    class Pipeline;
    class PipelineManager;
    class ProfilerGui;
    class Renderer;
    class Shader;
    struct PipelineCompileInput;

    namespace render_graph {
        class Resources;
    }

    class DeviceContext final : public sp::GraphicsContext {
    public:
        DeviceContext(GraphicsManager &graphics, bool enableValidationLayers = false);
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
            return instance;
        }

        vk::Queue &GetQueue(CommandContextType type) {
            return queues[QueueType(type)];
        }

        // Potential GraphicsContext function implementations
        bool BeginFrame() override;
        void SwapBuffers() override;
        void EndFrame() override;
        void WaitIdle() override {
            ZoneScoped;
            if (!deviceResetRequired) device->waitIdle();
        }
        bool RequiresReset() const override {
            return deviceResetRequired;
        }

        void AttachWindow(const std::shared_ptr<GuiContext> &context) override;

        void InitRenderer(Game &game) override;
        void Shutdown() override;
        std::shared_ptr<Renderer> GetRenderer() const;

        GenericCompositor &GetCompositor() override;

        void RenderFrame(chrono_clock::duration elapsedTime) override;

        // Returns a CommandContext that can be recorded and submitted within the current frame.
        // The each frame's CommandPool will be reset at the beginning of the frame.
        CommandContextPtr GetFrameCommandContext(render_graph::Resources &resources,
            CommandContextType type = CommandContextType::General);

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

        AsyncPtr<Buffer> CreateUploadBuffer(const InitialData &data,
            vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eTransferSrc);
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
        AsyncPtr<Image> CreateImage(ImageCreateInfo createInfo, const InitialData &initialData = {});
        AsyncPtr<Image> CreateImage(ImageCreateInfo createInfo, const AsyncPtr<Buffer> &uploadBuffer);
        AsyncPtr<Image> UpdateImage(const AsyncPtr<Image> &dstImage,
            const AsyncPtr<Buffer> &srcData,
            bool updateMipmap = true);
        AsyncPtr<Image> UpdateImageMipmap(const AsyncPtr<Image> &image);
        ImageViewPtr CreateImageView(ImageViewCreateInfo info);
        AsyncPtr<ImageView> CreateImageAndView(const ImageCreateInfo &imageInfo,
            const ImageViewCreateInfo &viewInfo, // image field is filled in automatically
            const InitialData &data = {});
        ImageViewPtr SwapchainImageView();
        vk::Sampler GetSampler(SamplerType type);
        vk::Sampler GetSampler(const vk::SamplerCreateInfo &info);

        AsyncPtr<ImageView> LoadAssetImage(string_view assetName, bool genMipmap = false, bool srgb = true);
        AsyncPtr<ImageView> LoadImage(shared_ptr<const sp::Image> image, bool genMipmap = false, bool srgb = true);

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
        std::shared_ptr<vk::UniqueSemaphore> GetEmptySemaphore(vk::Fence inUseUntilFence);

        using TemporaryObject = std::variant<BufferPtr, ImagePtr, ImageViewPtr, std::shared_ptr<vk::UniqueSemaphore>>;
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

#ifdef TRACY_ENABLE_GRAPHICS
        tracy::VkCtx *GetTracyContext(CommandContextType type);
#endif

        uint32 QueueFamilyIndex(CommandContextType type) const {
            return queueFamilyIndex[QueueType(type)];
        }

        void *Win32WindowHandle() override;

#if VK_HEADER_VERSION >= 304
        vk::Flags<vk::DebugUtilsMessageTypeFlagBitsEXT> disabledDebugMessages = {};
#else
        VkDebugUtilsMessageTypeFlagsEXT disabledDebugMessages = 0;
#endif

        void FlushMainQueue(bool blockUntilReady = true) {
            frameEndQueue.Flush(blockUntilReady);
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

    private:
        void CreateSwapchain();
        void CreateTestPipeline();
        void RecreateSwapchain();

        void PrepareResourcesForFrame();

        shared_ptr<Shader> CreateShader(const string &name, Hash64 compareHash);

        GraphicsManager &graphics;

        std::thread::id mainThread;
        std::thread::id renderThread;

        vk::Instance instance;
        vk::UniqueDebugUtilsMessengerEXT debugMessenger;
        vk::PhysicalDevice physicalDevice;
        vk::PhysicalDeviceProperties2 physicalDeviceProperties;
        vk::PhysicalDeviceDescriptorIndexingProperties physicalDeviceDescriptorIndexingProperties;
        vk::UniqueDevice device;
        unique_ptr<VmaAllocator_T, void (*)(VmaAllocator)> allocator;
        vk::SurfaceKHR surface;

        unique_ptr<PerfTimer> perfTimer;
        shared_ptr<ProfilerGui> profilerGui;

#ifdef TRACY_ENABLE_GRAPHICS
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

        struct SwapchainImageContext {
            vk::Fence inFlightFence; // points at a fence owned by FrameContext
            ImageViewPtr imageView;
        };

        vector<SwapchainImageContext> swapchainImageContexts;
        uint32 swapchainImageIndex = 0;

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

        bool deviceResetRequired = false;

        robin_hood::unordered_map<SamplerType, vk::UniqueSampler> namedSamplers;

        using SamplerKey = HashKey<VkSamplerCreateInfo>;
        robin_hood::unordered_map<SamplerKey, vk::UniqueSampler, SamplerKey::Hasher> adhocSamplers;

        bool systemFullscreen = false;
        glm::ivec2 systemWindowSize = glm::ivec2(0);
        glm::ivec4 storedWindowRect = glm::ivec4(0); // Remember window position and size when returning from fullscreen
        double lastFrameEnd = 0, fpsTimer = 0;
        uint32 frameCounter = 0, frameCounterThisSecond = 0;
        std::atomic_uint32_t measuredFrameRate;

        DispatchQueue frameBeginQueue, frameEndQueue, allocatorQueue;

        std::unique_ptr<CFuncCollection> funcs;

        rg::RenderGraph graph;
        std::unique_ptr<vulkan::Compositor> compositor;
        std::shared_ptr<vulkan::Renderer> vkRenderer;
    };
} // namespace sp::vulkan
