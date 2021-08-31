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

    class CommandContext : public NonCopyable {
    public:
        using DirtyFlags = CommandContextFlags::DirtyFlags;
        using DirtyBits = CommandContextFlags::DirtyBits;

        CommandContext(DeviceContext &device);
        ~CommandContext();

        // A CommandContext MUST be submitted to the device or abandoned before destroying the object.
        void Abandon();

        void BeginRenderPass(const RenderPassInfo &info);
        void EndRenderPass();

        void Draw(uint32 vertexes, uint32 instances, int32 firstVertex, uint32 firstInstance);
        void DrawIndexed(uint32 indexes, uint32 instances, uint32 firstIndex, int32 vertexOffset, uint32 firstInstance);

        void SetShader(ShaderStage stage, ShaderHandle handle);
        void SetShader(ShaderStage stage, string name);
        void PushConstants(const void *data, VkDeviceSize offset, VkDeviceSize range);

        void SetDefaultOpaqueState();

        void SetDepthTest(bool test, bool write) {
            pipelineState.state.values.depthTest = test;
            pipelineState.state.values.depthWrite = write;
            SetDirty(DirtyBits::Pipeline);
        }

        void SetStencilTest(bool test) {
            pipelineState.state.values.stencilTest = test;
            SetDirty(DirtyBits::Pipeline);
        }

        void SetBlending(bool enable) {
            pipelineState.state.values.blendEnable = enable;
            SetDirty(DirtyBits::Pipeline);
        }

        void FlushGraphicsState();

        vk::CommandBuffer &Raw() {
            return *cmd;
        }

    protected:
        friend class DeviceContext;
        void Begin();
        void End();

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

        bool recording = false, abandoned = false;

        vk::UniqueCommandBuffer cmd;
        vk::Viewport viewport;
        vk::Rect2D scissor;

        PipelineCompileInput pipelineState;
        PipelineDynamicState dynamicState;
        DeviceContext &device;

        shared_ptr<Pipeline> currentPipeline;

        shared_ptr<RenderPass> renderPass;
        shared_ptr<Framebuffer> framebuffer;

        DirtyFlags dirty;

        ShaderDataBindings shaderData;
    };
} // namespace sp::vulkan
