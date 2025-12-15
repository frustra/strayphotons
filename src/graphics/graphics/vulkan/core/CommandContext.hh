/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/InlineVector.hh"
#include "graphics/vulkan/core/Pipeline.hh"
#include "graphics/vulkan/core/RenderPass.hh"
#include "graphics/vulkan/core/Util.hh"

#include <bit>
#include <functional>
#include <glm/glm.hpp>
#include <robin_hood.h>
#include <vulkan/vulkan.hpp>

namespace sp::vulkan::CommandContextFlags {
    enum class DirtyFlags {
        Viewport = 1 << 0,
        Scissor = 1 << 1,
        PushConstants = 1 << 2,
        Pipeline = 1 << 3,
        Stencil = 1 << 4,
    };
} // namespace sp::vulkan::CommandContextFlags

template<>
struct magic_enum::customize::enum_range<sp::vulkan::CommandContextFlags::DirtyFlags> {
    static constexpr bool is_flags = true;
};

namespace sp::vulkan {
    class Model;
    struct VertexLayout;

    namespace render_graph {
        class Resources;
        typedef uint32 ResourceID;
    } // namespace render_graph

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

    const size_t MAX_VIEWPORTS = 4;

    class CommandContext : public NonCopyable {
    public:
        using DirtyFlags = CommandContextFlags::DirtyFlags;

        CommandContext(DeviceContext &device,
            vk::UniqueCommandBuffer cmd,
            CommandContextType type,
            CommandContextScope scope) noexcept;
        ~CommandContext();

        CommandContextType GetType() {
            return type;
        }

        // A CommandContext MUST be submitted to the device or abandoned before destroying the object.
        void Abandon();

        void Reset();
        void BeginRenderPass(const RenderPassInfo &info);
        void EndRenderPass();

        void PushConstants(const void *data, VkDeviceSize offset, VkDeviceSize range);

        template<typename T>
        void PushConstants(const T &data, VkDeviceSize offset = 0) {
            PushConstants(&data, offset, sizeof(T));
        }

        void Dispatch(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ);
        // indirectBuffer stores a VkDispatchIndirectCommand object
        void DispatchIndirect(BufferPtr indirectBuffer, vk::DeviceSize offset);
        void Draw(uint32 vertexes, uint32 instances = 1, int32 firstVertex = 0, uint32 firstInstance = 0);
        void DrawIndexed(uint32 indexes,
            uint32 instances = 1,
            uint32 firstIndex = 0,
            int32 vertexOffset = 0,
            uint32 firstInstance = 0);
        void DrawIndirect(BufferPtr drawCommands,
            vk::DeviceSize offset,
            uint32 drawCount,
            uint32 stride = sizeof(VkDrawIndirectCommand));
        void DrawIndirectCount(BufferPtr drawCommands,
            vk::DeviceSize offset,
            BufferPtr countBuffer,
            vk::DeviceSize countOffset,
            uint32 maxDrawCount,
            uint32 stride = sizeof(VkDrawIndirectCommand));
        void DrawIndexedIndirect(BufferPtr drawCommands,
            vk::DeviceSize offset,
            uint32 drawCount,
            uint32 stride = sizeof(VkDrawIndexedIndirectCommand));
        void DrawIndexedIndirectCount(BufferPtr drawCommands,
            vk::DeviceSize offset,
            BufferPtr countBuffer,
            vk::DeviceSize countOffset,
            uint32 maxDrawCount,
            uint32 stride = sizeof(VkDrawIndexedIndirectCommand));

        void DrawScreenCover(const ImageViewPtr &view = nullptr);

        void ImageBarrier(const ImagePtr &image,
            vk::ImageLayout oldLayout, // Transition the image from oldLayout
            vk::ImageLayout newLayout, // to newLayout,
            vk::PipelineStageFlags srcStages, // ensuring any image accesses in these stages
            vk::AccessFlags srcAccess, // of these types (usually writes) are complete and visible.
            vk::PipelineStageFlags dstStages, // Block work in these stages until the transition is complete,
            vk::AccessFlags dstAccess, // but only block these access types (can be reads or writes).
            const ImageBarrierInfo &options = {});

        void ImageBarrier(const ImagePtr &image,
            vk::ImageLayout newLayout, // Transition to this layout, ensuring the last image access is complete.
            vk::PipelineStageFlags dstStages, // Block work in these stages until the transition is complete,
            vk::AccessFlags dstAccess, // but only block these access types (can be reads or writes).
            const ImageBarrierInfo &options = {});

        // Sets the shaders for arbitrary stages. Any stages not defined in `shaders` will be unset.
        void SetShaders(std::initializer_list<std::pair<ShaderStage, string_view>> shaders);
        // Sets the shaders to a standard vertex+fragment pipeline.
        void SetShaders(string_view vertexName, string_view fragName);
        // Sets the shaders to a standard compute pipeline.
        void SetComputeShader(string_view name);

        void SetShaderConstant(ShaderStage stage, string_view name, uint32 data);

        template<typename T, std::enable_if_t<sizeof(T) == sizeof(uint32), int> = 0>
        void SetShaderConstant(ShaderStage stage, string_view name, T data) {
            SetShaderConstant(stage, name, std::bit_cast<uint32>(data));
        }

        void SetShaderConstant(ShaderStage stage, string_view name, bool data) {
            SetShaderConstant(stage, name, (uint32)data);
        }

        void SetDefaultOpaqueState();

        void SetVertexLayout(const VertexLayout &layout) {
            if (layout != pipelineInput.state.vertexLayout) {
                pipelineInput.state.vertexLayout = layout;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetPrimitiveTopology(vk::PrimitiveTopology topology) {
            if (topology != pipelineInput.state.primitiveTopology) {
                pipelineInput.state.primitiveTopology = topology;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetPolygonMode(vk::PolygonMode mode) {
            if (mode != pipelineInput.state.polygonMode) {
                pipelineInput.state.polygonMode = mode;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetLineWidth(float width) {
            if (width != pipelineInput.state.lineWidth) {
                pipelineInput.state.lineWidth = width;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetScissor(const vk::Rect2D &newScissor) {
            if (pipelineInput.state.scissorCount != 1) {
                pipelineInput.state.scissorCount = 1;
                SetDirty(DirtyFlags::Pipeline);
            }
            if (scissors[0] != newScissor) {
                scissors[0] = newScissor;
                SetDirty(DirtyFlags::Scissor);
            }
        }

        void SetScissorArray(vk::ArrayProxy<const vk::Rect2D> newScissors);

        void ClearScissor() {
            vk::Rect2D framebufferExtents = {{0, 0}, framebuffer->Extent()};
            SetScissor(framebufferExtents);
        }

        vk::Extent2D GetFramebufferExtent() const {
            if (!framebuffer) return {};
            return framebuffer->Extent();
        }

        void SetYDirection(YDirection dir) {
            if (viewportYDirection != dir) {
                viewportYDirection = dir;
                SetDirty(DirtyFlags::Viewport);

                if (dir == YDirection::Down) {
                    SetFrontFaceWinding(vk::FrontFace::eClockwise);
                } else {
                    SetFrontFaceWinding(vk::FrontFace::eCounterClockwise);
                }
            }
        }

        void SetViewport(const vk::Rect2D &newViewport) {
            if (pipelineInput.state.viewportCount != 1) {
                pipelineInput.state.viewportCount = 1;
                SetDirty(DirtyFlags::Pipeline);
            }
            if (viewports[0] != newViewport) {
                viewports[0] = newViewport;
                SetDirty(DirtyFlags::Viewport);
            }
        }

        void SetViewportArray(vk::ArrayProxy<const vk::Rect2D> newViewports);

        void SetDepthRange(float minDepth, float maxDepth) {
            this->minDepth = minDepth;
            this->maxDepth = maxDepth;
            SetDirty(DirtyFlags::Viewport);
        }

        void SetDepthTest(bool test, bool write) {
            if (test != pipelineInput.state.depthTest || write != pipelineInput.state.depthWrite) {
                pipelineInput.state.depthTest = test;
                pipelineInput.state.depthWrite = write;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetDepthCompareOp(vk::CompareOp compareOp) {
            if (compareOp != pipelineInput.state.depthCompareOp) {
                pipelineInput.state.depthCompareOp = compareOp;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetStencilTest(bool test) {
            if (test != pipelineInput.state.stencilTest) {
                pipelineInput.state.stencilTest = test;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetStencilWriteMask(vk::StencilFaceFlags faces, uint32 mask) {
            if (faces & vk::StencilFaceFlagBits::eFront) {
                if (mask != stencilState[0].writeMask) {
                    stencilState[0].writeMask = mask;
                    SetDirty(DirtyFlags::Stencil);
                }
            }
            if (faces & vk::StencilFaceFlagBits::eBack) {
                if (mask != stencilState[1].writeMask) {
                    stencilState[1].writeMask = mask;
                    SetDirty(DirtyFlags::Stencil);
                }
            }
        }

        void SetStencilCompareMask(vk::StencilFaceFlags faces, uint32 mask) {
            if (faces & vk::StencilFaceFlagBits::eFront) {
                if (mask != stencilState[0].compareMask) {
                    stencilState[0].compareMask = mask;
                    SetDirty(DirtyFlags::Stencil);
                }
            }
            if (faces & vk::StencilFaceFlagBits::eBack) {
                if (mask != stencilState[1].compareMask) {
                    stencilState[1].compareMask = mask;
                    SetDirty(DirtyFlags::Stencil);
                }
            }
        }

        void SetStencilReference(vk::StencilFaceFlags faces, uint32 value) {
            if (faces & vk::StencilFaceFlagBits::eFront) {
                if (value != stencilState[0].reference) {
                    stencilState[0].reference = value;
                    SetDirty(DirtyFlags::Stencil);
                }
            }
            if (faces & vk::StencilFaceFlagBits::eBack) {
                if (value != stencilState[1].reference) {
                    stencilState[1].reference = value;
                    SetDirty(DirtyFlags::Stencil);
                }
            }
        }

        void SetStencilCompareOp(vk::CompareOp op) {
            if (op != pipelineInput.state.stencilCompareOp) {
                pipelineInput.state.stencilCompareOp = op;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetStencilFailOp(vk::StencilOp op) {
            if (op != pipelineInput.state.stencilFailOp) {
                pipelineInput.state.stencilFailOp = op;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetStencilDepthFailOp(vk::StencilOp op) {
            if (op != pipelineInput.state.stencilDepthFailOp) {
                pipelineInput.state.stencilDepthFailOp = op;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetStencilPassOp(vk::StencilOp op) {
            if (op != pipelineInput.state.stencilPassOp) {
                pipelineInput.state.stencilPassOp = op;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetCullMode(vk::CullModeFlags mode) {
            if (mode != pipelineInput.state.cullMode) {
                pipelineInput.state.cullMode = mode;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetFrontFaceWinding(vk::FrontFace winding) {
            if (winding != pipelineInput.state.frontFaceWinding) {
                pipelineInput.state.frontFaceWinding = winding;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetBlending(bool enable, vk::BlendOp blendOp = vk::BlendOp::eAdd) {
            if (!enable) blendOp = vk::BlendOp::eAdd;
            if (enable != pipelineInput.state.blendEnable || blendOp != pipelineInput.state.blendOp) {
                pipelineInput.state.blendEnable = enable;
                pipelineInput.state.blendOp = blendOp;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetBlendFunc(vk::BlendFactor srcRGB,
            vk::BlendFactor dstRGB,
            vk::BlendFactor srcAlpha,
            vk::BlendFactor dstAlpha) {
            // TODO: This is a noop input for BlendOp::eMin or BlendOp::eMax
            if (srcRGB != pipelineInput.state.srcBlendFactor || dstRGB != pipelineInput.state.dstBlendFactor ||
                srcAlpha != pipelineInput.state.srcAlphaBlendFactor ||
                dstAlpha != pipelineInput.state.dstAlphaBlendFactor) {
                pipelineInput.state.srcBlendFactor = srcRGB;
                pipelineInput.state.srcAlphaBlendFactor = srcAlpha;
                pipelineInput.state.dstBlendFactor = dstRGB;
                pipelineInput.state.dstAlphaBlendFactor = dstAlpha;
                SetDirty(DirtyFlags::Pipeline);
            }
        }

        void SetImageView(uint32 set, uint32 binding, const ImageViewPtr &view);
        void SetImageView(uint32 set, uint32 binding, const ImageView *view);
        void SetImageView(string_view bindingName, const ImageViewPtr &view);
        void SetImageView(string_view bindingName, const ImageView *view);
        void SetImageView(string_view bindingName, render_graph::ResourceID resourceID);
        void SetImageView(string_view bindingName, string_view resourceName);
        void SetSampler(uint32 set, uint32 binding, const vk::Sampler &sampler);
        void SetSampler(string_view bindingName, const vk::Sampler &sampler);

        // Binds a buffer as to a uniform descriptor. Defaults to the whole buffer.
        void SetUniformBuffer(uint32 set,
            uint32 binding,
            const BufferPtr &buffer,
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = 0);
        void SetUniformBuffer(string_view bindingName,
            const BufferPtr &buffer,
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = 0);
        void SetUniformBuffer(string_view bindingName,
            render_graph::ResourceID resourceID,
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = 0);
        void SetUniformBuffer(string_view bindingName,
            string_view resourceName,
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = 0);

        // Binds a buffer as to a storage descriptor. Defaults to the whole buffer.
        void SetStorageBuffer(uint32 set,
            uint32 binding,
            const BufferPtr &buffer,
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = 0);
        void SetStorageBuffer(string_view bindingName,
            const BufferPtr &buffer,
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = 0);
        void SetStorageBuffer(string_view bindingName,
            render_graph::ResourceID resourceID,
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = 0);
        void SetStorageBuffer(string_view bindingName,
            string_view resourceName,
            vk::DeviceSize offset = 0,
            vk::DeviceSize range = 0);

        // Buffer is stored in a pool for this frame, and reused in later frames.
        BufferPtr AllocUniformBuffer(uint32 set, uint32 binding, vk::DeviceSize size);
        BufferPtr AllocUniformBuffer(string_view bindingName, vk::DeviceSize size);

        // Returns a CPU mapped pointer to the GPU buffer, valid at least until the CommandContext is submitted
        template<typename T>
        T *AllocUniformData(uint32 set, uint32 binding, uint32 count = 1) {
            auto buffer = AllocUniformBuffer(set, binding, sizeof(T) * count);
            return static_cast<T *>(buffer->Mapped());
        }

        template<typename T>
        void UploadUniformData(uint32 set, uint32 binding, const T *data, uint32 count = 1) {
            auto buffer = AllocUniformBuffer(set, binding, sizeof(T) * count);
            buffer->CopyFrom(data, count);
        }

        template<typename T>
        void UploadUniformData(string_view bindingName, const T *data, uint32 count = 1) {
            auto buffer = AllocUniformBuffer(bindingName, sizeof(T) * count);
            buffer->CopyFrom(data, count);
        }

        void SetBindlessDescriptors(uint32 set, vk::DescriptorSet descriptorSet);

        bool WritesToSwapchain() {
            return writesToSwapchain;
        }

        bool IsCompute() const {
            return pipelineInput.state.shaders[ShaderStage::Compute] > 0;
        }

        void FlushDescriptorSets(vk::PipelineBindPoint bindPoint);
        void FlushPushConstants();
        void FlushComputeState();
        void FlushGraphicsState();

        vk::CommandBuffer &Raw() {
            return *cmd;
        }

        DeviceContext &Device() {
            return device;
        }

        vk::Fence Fence();

    protected:
        friend class DeviceContext;
        void Begin(render_graph::Resources *resources = nullptr);
        void End();

        vk::UniqueCommandBuffer &RawRef() {
            return cmd;
        }

    private:
        void SetSingleShader(ShaderStage stage, ShaderHandle handle);
        void SetSingleShader(ShaderStage stage, string_view name);
        void SetShaderConstant(ShaderStage stage, uint32 index, uint32 data);

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
        CommandContextScope scope;
        render_graph::Resources *resources;

        vk::UniqueFence fence;

        bool recording = false, abandoned = false;

        YDirection viewportYDirection = YDirection::Up;
        std::array<vk::Rect2D, MAX_VIEWPORTS> viewports;
        std::array<vk::Rect2D, MAX_VIEWPORTS> scissors;
        float minDepth = 0.0f, maxDepth = 1.0f;

        struct StencilDynamicState {
            uint32 writeMask = 0, compareMask = 0, reference = 0;
        } stencilState[2] = {}; // front and back

        PipelineCompileInput pipelineInput;
        shared_ptr<Pipeline> currentPipeline;

        shared_ptr<RenderPass> renderPass;
        shared_ptr<Framebuffer> framebuffer;
        bool writesToSwapchain = false;

        DirtyFlags dirty;
        uint32 dirtyDescriptorSets = 0;

        ShaderDataBindings shaderData;
        std::array<vk::DescriptorSet, MAX_BOUND_DESCRIPTOR_SETS> bindlessSets;
    };
} // namespace sp::vulkan
