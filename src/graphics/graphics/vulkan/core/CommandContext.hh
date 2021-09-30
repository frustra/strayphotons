#pragma once

#include "core/Common.hh"
#include "graphics/vulkan/core/Pipeline.hh"
#include "graphics/vulkan/core/RenderPass.hh"

#include <functional>
#include <glm/glm.hpp>
#include <robin_hood.h>
#include <vulkan/vulkan.hpp>

namespace sp::vulkan::CommandContextFlags {
    enum class DirtyBits {
        Viewport = 1 << 0,
        Scissor = 1 << 1,
        PushConstants = 1 << 2,
        Pipeline = 1 << 3,
    };

    using DirtyFlags = vk::Flags<DirtyBits>;
} // namespace sp::vulkan::CommandContextFlags

template<>
struct vk::FlagTraits<sp::vulkan::CommandContextFlags::DirtyBits> {
    enum : sp::vulkan::CommandContextFlags::DirtyFlags::MaskType { allFlags = ~0 };
};

struct ImageBarrierInfo {
    uint32 baseMipLevel = 0;
    uint32 mipLevelCount = 0; // default: use all levels
    uint32 baseArrayLayer = 0;
    uint32 arrayLayerCount = 0; // default: use all layers
    uint32 srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    uint32 dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    // false skips checking and saving the image layout,
    // caller must set the image's layout before passing the image to other code
    bool trackImageLayout = true;
};

namespace sp::vulkan {
    class Model;
    struct VertexLayout;

    class CommandContext : public NonCopyable {
    public:
        using DirtyFlags = CommandContextFlags::DirtyFlags;
        using DirtyBits = CommandContextFlags::DirtyBits;

        CommandContext(DeviceContext &device, vk::UniqueCommandBuffer cmd, CommandContextType type) noexcept;
        ~CommandContext();

        CommandContextType GetType() {
            return type;
        }

        // A CommandContext MUST be submitted to the device or abandoned before destroying the object.
        void Abandon();

        void BeginRenderPass(const RenderPassInfo &info);
        void EndRenderPass();

        void Draw(uint32 vertexes, uint32 instances = 1, int32 firstVertex = 0, uint32 firstInstance = 0);
        void DrawIndexed(uint32 indexes,
                         uint32 instances = 1,
                         uint32 firstIndex = 0,
                         int32 vertexOffset = 0,
                         uint32 firstInstance = 0);
        void DrawScreenCover(const ImageViewPtr &view = nullptr);

        void ImageBarrier(
            const ImagePtr &image,
            vk::ImageLayout oldLayout, // Transition the image from oldLayout
            vk::ImageLayout newLayout, // to newLayout,
            vk::PipelineStageFlags srcStages, // ensuring any image accesses in these stages
            vk::AccessFlags srcAccess, // of these types (usually writes) are complete and visible.
            vk::PipelineStageFlags dstStages, // Block work in these stages until the transition is complete,
            vk::AccessFlags dstAccess, // but only block these access types (can be reads or writes).
            const ImageBarrierInfo &options = {});

        void SetShaders(const string &vertexName, const string &fragName);
        void SetShader(ShaderStage stage, ShaderHandle handle);
        void SetShader(ShaderStage stage, const string &name);
        void PushConstants(const void *data, VkDeviceSize offset, VkDeviceSize range);

        void SetDefaultOpaqueState();

        void SetVertexLayout(const VertexLayout &layout) {
            if (layout != pipelineInput.state.vertexLayout) {
                pipelineInput.state.vertexLayout = layout;
                SetDirty(DirtyBits::Pipeline);
            }
        }

        void SetScissor(const vk::Rect2D &newScissor) {
            if (scissor != newScissor) {
                scissor = newScissor;
                SetDirty(DirtyBits::Scissor);
            }
        }

        void ClearScissor() {
            vk::Rect2D framebufferExtents = {{0, 0}, framebuffer->Extent()};
            SetScissor(framebufferExtents);
        }

        vk::Extent2D GetFramebufferExtent() const {
            if (!framebuffer) return {};
            return framebuffer->Extent();
        }

        vk::Rect2D GetViewport() const {
            return viewport;
        }

        void SetViewport(const vk::Rect2D &newViewport) {
            if (viewport != newViewport) {
                viewport = newViewport;
                SetDirty(DirtyBits::Viewport);
            }
        }

        void SetDepthRange(float minDepth, float maxDepth) {
            this->minDepth = minDepth;
            this->maxDepth = maxDepth;
            SetDirty(DirtyBits::Viewport);
        }

        void SetDepthTest(bool test, bool write) {
            if (test != pipelineInput.state.depthTest || write != pipelineInput.state.depthWrite) {
                pipelineInput.state.depthTest = test;
                pipelineInput.state.depthWrite = write;
                SetDirty(DirtyBits::Pipeline);
            }
        }

        void SetStencilTest(bool test) {
            if (test != pipelineInput.state.stencilTest) {
                pipelineInput.state.stencilTest = test;
                SetDirty(DirtyBits::Pipeline);
            }
        }

        void SetCullMode(vk::CullModeFlags mode) {
            if (mode != pipelineInput.state.cullMode) {
                pipelineInput.state.cullMode = mode;
                SetDirty(DirtyBits::Pipeline);
            }
        }

        void SetBlending(bool enable, vk::BlendOp blendOp = vk::BlendOp::eAdd) {
            if (!enable) blendOp = vk::BlendOp::eAdd;
            if (enable != pipelineInput.state.blendEnable || blendOp != pipelineInput.state.blendOp) {
                pipelineInput.state.blendEnable = enable;
                pipelineInput.state.blendOp = blendOp;
                SetDirty(DirtyBits::Pipeline);
            }
        }

        void SetBlendFunc(vk::BlendFactor srcFactor, vk::BlendFactor dstFactor) {
            if (srcFactor != pipelineInput.state.srcBlendFactor || dstFactor != pipelineInput.state.dstBlendFactor) {
                pipelineInput.state.srcBlendFactor = srcFactor;
                pipelineInput.state.dstBlendFactor = dstFactor;
                SetDirty(DirtyBits::Pipeline);
            }
        }

        void SetTexture(uint32 set, uint32 binding, const ImageViewPtr &view); // uses view's default sampler
        void SetTexture(uint32 set, uint32 binding, const ImageView *view); // uses view's default sampler
        void SetTexture(uint32 set, uint32 binding, const vk::ImageView &view, SamplerType samplerType);
        void SetTexture(uint32 set, uint32 binding, const vk::ImageView &view, const vk::Sampler &sampler);
        void SetTexture(uint32 set, uint32 binding, const vk::ImageView &view);
        void SetSampler(uint32 set, uint32 binding, const vk::Sampler &sampler);

        void SetUniformBuffer(uint32 set, uint32 binding, const BufferPtr &buffer);

        // Buffer is stored in a pool for this frame, and reused in later frames.
        BufferPtr AllocUniformBuffer(uint32 set, uint32 binding, vk::DeviceSize size);

        // Returns a CPU mapped pointer to the GPU buffer, valid at least until the CommandContext is submitted
        template<typename T>
        T *AllocUniformData(uint32 set, uint32 binding, uint32 count = 1) {
            auto buffer = AllocUniformBuffer(set, binding, sizeof(T) * count);
            T *data;
            buffer->MapPersistent((void **)&data);
            return data;
        }

        template<typename T>
        void UploadUniformData(uint32 set, uint32 binding, const T *data, uint32 count = 1) {
            auto buffer = AllocUniformBuffer(set, binding, sizeof(T) * count);
            buffer->CopyFrom(data, count);
        }

        bool WritesToSwapchain() {
            return writesToSwapchain;
        }

        void FlushDescriptorSets();
        void FlushGraphicsState();

        template<typename Func>
        void AfterSubmit(Func func) {
            afterSubmitFuncs.push_back(func);
        }

        vk::CommandBuffer &Raw() {
            return *cmd;
        }

        DeviceContext &Device() {
            return device;
        }

    protected:
        friend class DeviceContext;
        void Begin();
        void End();

        vk::UniqueCommandBuffer &RawRef() {
            return cmd;
        }

    private:
        bool TestDirty(DirtyFlags flags) {
            return static_cast<bool>(dirty & flags);
        }

        bool ResetDirty(DirtyFlags flags) {
            bool ret = TestDirty(flags);
            dirty &= ~flags;
            return ret;
        }

        void SetDirty(DirtyFlags flags) {
            dirty |= flags;
        }

        bool TestDescriptorDirty(uint32 set) {
            return static_cast<bool>(dirtyDescriptorSets & (1 << set));
        }

        bool ResetDescriptorDirty(uint32 set) {
            bool ret = TestDescriptorDirty(set);
            dirtyDescriptorSets &= ~(1 << set);
            return ret;
        }

        void SetDescriptorDirty(uint32 set) {
            dirtyDescriptorSets |= (1 << set);
        }

        DeviceContext &device;
        vk::UniqueCommandBuffer cmd;
        CommandContextType type;

        bool recording = false, abandoned = false;

        vk::Rect2D viewport;
        float minDepth = 0.0f, maxDepth = 1.0f;
        vk::Rect2D scissor;

        PipelineCompileInput pipelineInput;
        shared_ptr<Pipeline> currentPipeline;

        shared_ptr<RenderPass> renderPass;
        shared_ptr<Framebuffer> framebuffer;
        bool writesToSwapchain = false;

        DirtyFlags dirty;
        uint32 dirtyDescriptorSets = 0;

        ShaderDataBindings shaderData;

        vector<std::function<void()>> afterSubmitFuncs;
    };
} // namespace sp::vulkan