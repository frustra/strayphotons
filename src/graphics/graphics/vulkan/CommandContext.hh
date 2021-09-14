#pragma once

#include "Pipeline.hh"
#include "RenderPass.hh"
#include "core/Common.hh"

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

namespace sp::vulkan {
    class DeviceContext;
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

        void Draw(uint32 vertexes, uint32 instances, int32 firstVertex, uint32 firstInstance);
        void DrawIndexed(uint32 indexes, uint32 instances, uint32 firstIndex, int32 vertexOffset, uint32 firstInstance);

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
            minDepth = minDepth;
            maxDepth = maxDepth;
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

        bool WritesToSwapchain() {
            return writesToSwapchain;
        }

        void FlushDescriptorSets();
        void FlushGraphicsState();

        vk::CommandBuffer &Raw() {
            return *cmd;
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
    };
} // namespace sp::vulkan
