/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "CommandContext.hh"

#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/Resources.hh"
#include "magic_enum.hpp"
#include "strayphotons/DispatchQueue.hh"
#include "strayphotons/Logging.hh"
#include "vulkan/vulkan.hpp"

#include <cstdint>
#include <mutex>
#include <string_view>
#include <vulkan/vulkan_core.h>

namespace sp::vulkan {
    CommandContext::CommandContext(DeviceContext &device,
        vk::CommandPool pool,
        QueueID queueId,
        CommandContextType type)
        : device(device), queueId(queueId), type(type) {
        vk::CommandBufferAllocateInfo allocInfo;
        allocInfo.commandPool = pool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = 1;
        auto buffers = device->allocateCommandBuffersUnique(allocInfo);
        cmd = std::move(buffers[0]);
        SetDefaultOpaqueState();
    }
    CommandContext::CommandContext(DeviceContext &device,
        vk::UniqueCommandBuffer cmd,
        QueueID queueId,
        CommandContextType type) noexcept
        : device(device), cmd(std::move(cmd)), queueId(queueId), type(type) {
        SetDefaultOpaqueState();
    }

    CommandContext::~CommandContext() {
        Assert(!recording, "command context was never ended/submitted");
    }

    void CommandContext::SetDefaultOpaqueState() {
        SetDepthTest(true, true);
        SetDepthRange(0.0f, 1.0f);
        SetColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        SetStencilTest(false);
        SetBlending(false);
        SetBlendFunc(vk::BlendFactor::eSrcAlpha,
            vk::BlendFactor::eOneMinusSrcAlpha,
            vk::BlendFactor::eOneMinusDstAlpha,
            vk::BlendFactor::eOne);
        SetCullMode(vk::CullModeFlagBits::eBack);
        SetFrontFaceWinding(vk::FrontFace::eCounterClockwise);
        SetPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
    }

    void CommandContext::Reset() {
        dirty = ~DirtyFlags();
        dirtyDescriptorSets = ~0u;
        currentPipeline = VK_NULL_HANDLE;
        pipelineInput.state.shaders = {};
        pipelineInput.renderPass = VK_NULL_HANDLE;
        framebuffer.reset();
        renderPass.reset();
    }

    void CommandContext::BeginRenderPass(const RenderPassInfo &info) {
        Reset();
        Assert(!framebuffer, "render pass already started");

        framebuffer = device.GetFramebuffer(info);

        pipelineInput.state.viewportCount = 1;
        pipelineInput.state.scissorCount = 1;
        viewports[0] = vk::Rect2D{{0, 0}, framebuffer->Extent()};
        scissors[0] = viewports[0];

        pipelineInput.renderPass = framebuffer->GetRenderPass();
        renderPass = device.GetRenderPass(info);

        vk::ClearValue clearValues[MAX_COLOR_ATTACHMENTS + 1];
        uint32_t clearValueCount = info.state.colorAttachmentCount;
        bool writesToSwapchain = false;
        for (uint32_t i = 0; i < info.state.colorAttachmentCount; i++) {
            if (info.state.ShouldClear(i)) {
                clearValues[i].color = info.clearColors[i];
            }
            if (info.colorAttachments[i]->IsSwapchain()) writesToSwapchain = true;
        }
        if (writesToSwapchain) {
            waitStages.emplace_back(vk::PipelineStageFlagBits::eColorAttachmentOutput);
            waitSemaphores.emplace_back(device.Frame().imageAvailableSemaphore);
            signalSemaphores.emplace_back(device.SwapchainImage().submitCompleteSemaphore);
        }

        if (info.HasDepthStencil() && info.state.ShouldClear(RenderPassState::DEPTH_STENCIL_INDEX)) {
            clearValues[info.state.colorAttachmentCount].depthStencil = info.clearDepthStencil;
            clearValueCount = info.state.colorAttachmentCount + 1;
        }

        vk::RenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.renderPass = *renderPass;
        renderPassBeginInfo.framebuffer = *framebuffer;
        renderPassBeginInfo.renderArea = scissors[0];
        renderPassBeginInfo.clearValueCount = clearValueCount;
        renderPassBeginInfo.pClearValues = clearValues;
        cmd->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

        renderPass->RecordImplicitImageLayoutTransitions(info);
    }

    void CommandContext::EndRenderPass() {
        Assert(!!framebuffer, "render pass not started");

        cmd->endRenderPass();
        Reset();
    }

    void CommandContext::Begin(render_graph::Resources *resources) {
        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd->begin(beginInfo);
        Assert(!recording.exchange(true), "command buffer already recording");
        this->resources = resources;
    }

    void CommandContext::End() {
        cmd->end();
        Assert(recording.exchange(false), "command buffer not recording");
    }

    void CommandContext::Dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
        FlushComputeState();
        cmd->dispatch(groupCountX, groupCountY, groupCountZ);
    }

    void CommandContext::DispatchIndirect(BufferPtr indirectBuffer, vk::DeviceSize offset) {
        Assert(indirectBuffer->IsAcquired(handle), "DispatchIndirect called without acquiring indirect buffer first");
        FlushComputeState();
        cmd->dispatchIndirect(*indirectBuffer, offset);
    }

    void CommandContext::Draw(uint32_t vertexes, uint32_t instances, int32_t firstVertex, uint32_t firstInstance) {
        FlushGraphicsState();
        cmd->draw(vertexes, instances, firstVertex, firstInstance);
    }

    void CommandContext::DrawIndexed(uint32_t indexes,
        uint32_t instances,
        uint32_t firstIndex,
        int32_t vertexOffset,
        uint32_t firstInstance) {
        FlushGraphicsState();
        cmd->drawIndexed(indexes, instances, firstIndex, vertexOffset, firstInstance);
    }

    void CommandContext::DrawIndirect(BufferPtr drawCommands,
        vk::DeviceSize offset,
        uint32_t drawCount,
        uint32_t stride) {
        Assert(drawCommands->IsAcquired(handle), "DrawIndirect called without acquiring draw commands buffer first");
        FlushGraphicsState();
        cmd->drawIndirect(*drawCommands, offset, drawCount, stride);
    }

    void CommandContext::DrawIndirectCount(BufferPtr drawCommands,
        vk::DeviceSize offset,
        BufferPtr countBuffer,
        vk::DeviceSize countOffset,
        uint32_t maxDrawCount,
        uint32_t stride) {
        Assert(drawCommands->IsAcquired(handle),
            "DrawIndirectCount called without acquiring draw commands buffer first");
        Assert(countBuffer->IsAcquired(handle), "DrawIndirectCount called without acquiring count buffer first");
        FlushGraphicsState();
        cmd->drawIndirectCount(*drawCommands, offset, *countBuffer, countOffset, maxDrawCount, stride);
    }

    void CommandContext::DrawIndexedIndirect(BufferPtr drawCommands,
        vk::DeviceSize offset,
        uint32_t drawCount,
        uint32_t stride) {
        Assert(drawCommands->IsAcquired(handle),
            "DrawIndexedIndirect called without acquiring draw commands buffer first");
        FlushGraphicsState();
        cmd->drawIndexedIndirect(*drawCommands, offset, drawCount, stride);
    }

    void CommandContext::DrawIndexedIndirectCount(BufferPtr drawCommands,
        vk::DeviceSize offset,
        BufferPtr countBuffer,
        vk::DeviceSize countOffset,
        uint32_t maxDrawCount,
        uint32_t stride) {
        Assert(drawCommands->IsAcquired(handle),
            "DrawIndexedIndirectCount called without acquiring draw commands buffer first");
        Assert(countBuffer->IsAcquired(handle), "DrawIndexedIndirectCount called without acquiring count buffer first");
        FlushGraphicsState();
        cmd->drawIndexedIndirectCount(*drawCommands, offset, *countBuffer, countOffset, maxDrawCount, stride);
    }

    void CommandContext::DrawScreenCover(const ImageViewPtr &view) {
        SetShaders("screen_cover.vert", "screen_cover.frag");
        if (view) {
            if (view->ViewType() == vk::ImageViewType::e2DArray) {
                SetSingleShader(ShaderStage::Fragment, "screen_cover_array.frag");
            }
            SetImageView("tex", view);
        }
        Draw(3); // vertices are defined as constants in the vertex shader
    }

    void CommandContext::DrawScreenCover(const color_alpha_t &color) {
        SetShaders("screen_cover.vert", "solid_color.frag");
        PushConstants((glm::vec4)color);
        Draw(3); // vertices are defined as constants in the vertex shader
    }

    void CommandContext::SetScissorArray(vk::ArrayProxy<const vk::Rect2D> newScissors) {
        Assert(newScissors.size() <= scissors.size(), "too many scissors");
        Assert(newScissors.size() <= device.Limits().maxViewports, "too many scissors for device");
        if (pipelineInput.state.scissorCount != newScissors.size()) {
            pipelineInput.state.scissorCount = newScissors.size();
            SetDirty(DirtyFlags::Pipeline);
        }
        size_t i = 0;
        for (auto &newScissor : newScissors) {
            if (scissors[i] != newScissor) {
                scissors[i] = newScissor;
                SetDirty(DirtyFlags::Scissor);
            }
            i++;
        }
    }

    void CommandContext::SetViewportArray(vk::ArrayProxy<const vk::Rect2D> newViewports) {
        Assert(newViewports.size() <= viewports.size(), "too many viewports");
        Assert(newViewports.size() <= device.Limits().maxViewports, "too many viewports for device");
        if (pipelineInput.state.viewportCount != newViewports.size()) {
            pipelineInput.state.viewportCount = newViewports.size();
            SetDirty(DirtyFlags::Pipeline);
        }
        size_t i = 0;
        for (auto &newViewport : newViewports) {
            if (viewports[i] != newViewport) {
                viewports[i] = newViewport;
                SetDirty(DirtyFlags::Viewport);
            }
            i++;
        }
    }

    void CommandContext::ImageBarrier(const ImagePtr &image,
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout,
        vk::PipelineStageFlags srcStages,
        vk::AccessFlags srcAccess,
        vk::PipelineStageFlags dstStages,
        vk::AccessFlags dstAccess,
        const ImageBarrierInfo &options) {
        Assert(std::this_thread::get_id() == device.renderThread, "ImageBarrier must be used in a single thread");

        vk::ImageMemoryBarrier barrier;
        barrier.image = *image;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        barrier.subresourceRange.aspectMask = FormatToAspectFlags(image->Format());
        barrier.subresourceRange.baseMipLevel = options.baseMipLevel;
        barrier.subresourceRange.levelCount = options.mipLevelCount ? options.mipLevelCount : image->MipLevels();
        barrier.subresourceRange.baseArrayLayer = options.baseArrayLayer;
        barrier.subresourceRange.layerCount = options.arrayLayerCount ? options.arrayLayerCount : image->ArrayLayers();
        barrier.srcQueueFamilyIndex = options.srcQueueFamilyIndex;
        barrier.dstQueueFamilyIndex = options.dstQueueFamilyIndex;
        cmd->pipelineBarrier(srcStages, dstStages, {}, {}, {}, {barrier});

        if (options.trackImageLayout) {
            Assert(
                !options.baseMipLevel && !options.mipLevelCount && !options.baseArrayLayer && !options.arrayLayerCount,
                "can't track image layout when specifying a subresource range");
            Assert(oldLayout == vk::ImageLayout::eUndefined || oldLayout == image->LastLayout(),
                "image had layout: " + vk::to_string(image->LastLayout()) + ", expected: " + vk::to_string(oldLayout));
            image->SetLayout(newLayout);
        }
    }

    void CommandContext::ImageBarrier(const ImagePtr &image,
        vk::ImageLayout newLayout,
        vk::PipelineStageFlags dstStages,
        vk::AccessFlags dstAccess,
        const ImageBarrierInfo &options) {
        Assertf(options.trackImageLayout, "CommandContext::ImageBarrier requires trackImageLayout or oldLayout");
        auto last = GetAccessInfo(image->lastAccess);
        if (last.stageMask == vk::PipelineStageFlags(0)) last.stageMask = vk::PipelineStageFlagBits::eTopOfPipe;
        ImageBarrier(image,
            image->LastLayout(),
            newLayout,
            last.stageMask,
            last.accessMask,
            dstStages,
            dstAccess,
            options);
    }

    void CommandContext::BufferBarrier(const BufferPtr &buffer,
        vk::PipelineStageFlags srcStages,
        vk::AccessFlags srcAccess,
        vk::PipelineStageFlags dstStages,
        vk::AccessFlags dstAccess,
        const BufferBarrierInfo &options) {
        Assert(std::this_thread::get_id() == device.renderThread, "BufferBarrier must be used in a single thread");

        vk::BufferMemoryBarrier barrier;
        barrier.buffer = *buffer;
        barrier.offset = 0;
        barrier.size = VK_WHOLE_SIZE;
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        barrier.srcQueueFamilyIndex = options.srcQueueFamilyIndex;
        barrier.dstQueueFamilyIndex = options.dstQueueFamilyIndex;
        cmd->pipelineBarrier(srcStages, dstStages, {}, {}, {barrier}, {});
    }

    void CommandContext::BufferBarrier(const BufferPtr &buffer,
        vk::PipelineStageFlags dstStages,
        vk::AccessFlags dstAccess,
        const BufferBarrierInfo &options) {
        auto last = GetAccessInfo(buffer->lastAccess);
        if (last.stageMask == vk::PipelineStageFlags(0)) last.stageMask = vk::PipelineStageFlagBits::eTopOfPipe;
        BufferBarrier(buffer, last.stageMask, last.accessMask, dstStages, dstAccess, options);
    }

    void CommandContext::SetShaders(std::initializer_list<std::pair<ShaderStage, std::string_view>> shaders) {
        pipelineInput.state.shaders = {};
        for (auto &s : shaders) {
            SetSingleShader(s.first, s.second);
        }
    }

    void CommandContext::SetShaders(std::string_view vertName, std::string_view fragName) {
        pipelineInput.state.shaders = {};
        SetSingleShader(ShaderStage::Vertex, vertName);
        SetSingleShader(ShaderStage::Fragment, fragName);
    }

    void CommandContext::SetComputeShader(std::string_view name) {
        pipelineInput.state.shaders = {};
        SetSingleShader(ShaderStage::Compute, name);
    }

    void CommandContext::SetSingleShader(ShaderStage stage, ShaderHandle handle) {
        auto &slot = pipelineInput.state.shaders[stage];
        if (slot == handle) return;
        slot = handle;

        auto &spec = pipelineInput.state.specializations[stage];
        std::fill(spec.values.begin(), spec.values.end(), 0);
        spec.set.reset();
        SetDirty(DirtyFlags::Pipeline);

        // TODO: Move autobinding to FlushDescriptorSets
        // auto shader = device.GetShader(slot);
        // if (shader) {
        //     for (auto &set : shader->descriptorSets) {
        //         for (auto &binding : set.bindings) {
        //             if (!binding.accessed) continue;
        //             // if (binding.type != vk::DescriptorType::eUniformBuffer) continue;

        //             if (starts_with(binding.name, "G_")) {
        //                 Assert(resources, "Render Graph resources not set on CommandContext");
        //                 auto id = resources->GetID(binding.name.substr(2), false);
        //                 if (binding.type == vk::DescriptorType::eUniformBuffer) {
        //                     if (id == render_graph::InvalidResource) {
        //                         Warnf("Shader(%s): tried to autobind missing resource: %s",
        //                             shader->name,
        //                             binding.name.substr(2));
        //                     }
        //                     SetUniformBuffer(set.setId, binding.bindingId, resources->GetBuffer(id));
        //                 } else {
        //                     Warnf("Shader(%s): autobinding type on %s is unimplemented: %s",
        //                         shader->name,
        //                         binding.name,
        //                         binding.type);
        //                 }
        //             }
        //         }
        //     }
        // }
    }

    void CommandContext::SetSingleShader(ShaderStage stage, std::string_view name) {
        SetSingleShader(stage, device.LoadShader(name));
    }

    void CommandContext::SetShaderConstant(ShaderStage stage, uint32_t index, uint32_t data) {
        Assert(pipelineInput.state.shaders[stage], "no shader bound to set constant");
        auto &spec = pipelineInput.state.specializations[stage];
        spec.values[index] = data;
        spec.set.set(index, true);
        SetDirty(DirtyFlags::Pipeline);
    }

    void CommandContext::SetShaderConstant(ShaderStage stage, std::string_view name, uint32_t data) {
        Assert(pipelineInput.state.shaders[stage], "no shader bound to set constant");
        auto shader = device.GetShader(pipelineInput.state.shaders[stage]);
        Assertf(shader, "bound shader is null when setting constant");
        uint32_t index = ~0u;
        for (auto &specConstant : shader->specConstants) {
            if (name == specConstant.name) {
                index = specConstant.constantId;
                break;
            }
        }
        auto &spec = pipelineInput.state.specializations[stage];
        if (index >= spec.values.size()) {
            Errorf("Shader constant %s not found on shader %s", name, shader->name);
            return;
        }
        spec.values[index] = data;
        spec.set.set(index, true);
        SetDirty(DirtyFlags::Pipeline);
    }

    void CommandContext::PushConstants(const void *data, VkDeviceSize offset, VkDeviceSize range) {
        Assert(offset + range <= sizeof(shaderData.pushConstants), "CommandContext::PushConstants overflow");
        memcpy(shaderData.pushConstants + offset, data, range);
        SetDirty(DirtyFlags::PushConstants);
    }

    void CommandContext::SetSampler(uint32_t set, uint32_t binding, const vk::Sampler &sampler) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        Assert(binding < MAX_BINDINGS_PER_DESCRIPTOR_SET, "binding index too high");
        auto &image = shaderData.sets[set].bindings[binding].image;
        image.sampler = sampler;
        SetDescriptorDirty(set);
    }

    void CommandContext::SetSampler(std::string_view bindingName, const vk::Sampler &sampler) {
        std::shared_ptr<Shader> lastShader;
        for (auto stage : magic_enum::enum_values<ShaderStage>()) {
            auto &slot = pipelineInput.state.shaders[stage];
            auto shader = device.GetShader(slot);
            if (!shader) continue;
            lastShader = shader;
            for (auto &set : shader->descriptorSets) {
                for (auto &binding : set.bindings) {
                    if (binding.type != vk::DescriptorType::eCombinedImageSampler) continue;
                    if (binding.name == bindingName) {
                        SetSampler(set.setId, binding.bindingId, sampler);
                        return;
                    }
                }
            }
        }
        Assert(lastShader, "SetSampler called with no shader bound");
        Errorf("SetSampler binding %s not found on any bound shader: (last: %s)", bindingName, lastShader->name);
    }

    void CommandContext::SetImageView(uint32_t set, uint32_t binding, const ImageViewPtr &view) {
        activeImageViews.emplace_back(view);
        SetImageView(set, binding, view.get());
    }

    void CommandContext::SetImageView(uint32_t set, uint32_t binding, const ImageView *view) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        Assert(binding < MAX_BINDINGS_PER_DESCRIPTOR_SET, "binding index too high");
        auto &bindingDesc = shaderData.sets[set].bindings[binding];
        bindingDesc.uniqueID = view->GetUniqueID();

        auto &image = view->Image();
        Assert(image->IsAcquired(handle), "SetImageView called without acquiring image first");
        DebugAssertf(image->LastLayout() == vk::ImageLayout::eShaderReadOnlyOptimal ||
                         image->LastLayout() == vk::ImageLayout::eDepthStencilReadOnlyOptimal ||
                         image->LastLayout() == vk::ImageLayout::eGeneral,
            "SetImageView image sampler has invalid layout: %s",
            image->LastLayout());

        bindingDesc.image.imageView = **view;
        bindingDesc.image.imageLayout = (VkImageLayout)image->LastLayout();
        SetDescriptorDirty(set);

        auto defaultSampler = view->DefaultSampler();
        if (defaultSampler) SetSampler(set, binding, defaultSampler);
    }

    void CommandContext::SetImageView(std::string_view bindingName, const ImageViewPtr &view) {
        activeImageViews.emplace_back(view);
        SetImageView(bindingName, view.get());
    }

    void CommandContext::SetImageView(std::string_view bindingName, const ImageView *view) {
        std::shared_ptr<Shader> lastShader;
        for (auto stage : magic_enum::enum_values<ShaderStage>()) {
            auto &slot = pipelineInput.state.shaders[stage];
            auto shader = device.GetShader(slot);
            if (!shader) continue;
            lastShader = shader;
            for (auto &set : shader->descriptorSets) {
                for (auto &binding : set.bindings) {
                    if (binding.type != vk::DescriptorType::eStorageImage &&
                        binding.type != vk::DescriptorType::eCombinedImageSampler)
                        continue;
                    if (binding.name == bindingName) {
                        SetImageView(set.setId, binding.bindingId, view);
                        return;
                    }
                }
            }
        }
        Assert(lastShader, "SetImageView called with no shader bound");
        Errorf("SetImageView binding %s not found on any bound shader: (last: %s)", bindingName, lastShader->name);
    }

    void CommandContext::SetImageView(std::string_view bindingName, render_graph::ResourceID resourceID) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetImageView(bindingName, resources->GetImageView(resourceID));
    }

    void CommandContext::SetImageView(std::string_view bindingName, std::string_view resourceName) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetImageView(bindingName, resources->GetImageView(resourceName));
    }

    static void checkBufferOffsets(const BufferPtr &buffer, vk::DeviceSize offset, vk::DeviceSize range) {
        Assertf(offset + range <= buffer->ByteSize(),
            "tried to bind past the end of a buffer, offset: %d, range: %d, size: %d",
            offset,
            range,
            buffer->ByteSize());
    }

    void CommandContext::SetUniformBuffer(uint32_t set,
        uint32_t binding,
        const BufferPtr &buffer,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        Assert(binding < MAX_BINDINGS_PER_DESCRIPTOR_SET, "binding index too high");
        Assert(buffer->IsAcquired(handle), "SetUniformBuffer called without acquiring buffer first");
        auto &bindingDesc = shaderData.sets[set].bindings[binding];
        bindingDesc.uniqueID = buffer ? buffer->GetUniqueID() : 0;

        auto &bufferBinding = bindingDesc.buffer;
        if (buffer) {
            bufferBinding.buffer = **buffer;
            bufferBinding.offset = offset;
            bufferBinding.range = range == 0 ? buffer->ByteSize() - offset : range;
            bindingDesc.arrayStride = buffer->ArrayStride();

            checkBufferOffsets(buffer, bufferBinding.offset, bufferBinding.range);
        } else {
            bufferBinding.buffer = nullptr;
            bufferBinding.offset = 0;
            bufferBinding.range = 0;
            bindingDesc.arrayStride = 0;
        }
        SetDescriptorDirty(set);
    }

    void CommandContext::SetUniformBuffer(std::string_view bindingName,
        const BufferPtr &buffer,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        std::shared_ptr<Shader> lastShader;
        for (auto stage : magic_enum::enum_values<ShaderStage>()) {
            auto &slot = pipelineInput.state.shaders[stage];
            auto shader = device.GetShader(slot);
            if (!shader) continue;
            lastShader = shader;
            for (auto &set : shader->descriptorSets) {
                for (auto &binding : set.bindings) {
                    if (binding.type != vk::DescriptorType::eUniformBuffer) continue;
                    if (binding.name == bindingName) {
                        SetUniformBuffer(set.setId, binding.bindingId, buffer, offset, range);
                        return;
                    }
                }
            }
        }
        Assert(lastShader, "SetUniformBuffer called with no shader bound");
        Errorf("SetUniformBuffer binding %s not found on any bound shader: (last: %s)", bindingName, lastShader->name);
    }

    void CommandContext::SetUniformBuffer(std::string_view bindingName,
        std::string_view resourceName,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetUniformBuffer(bindingName, resources->GetBuffer(resourceName), offset, range);
    }

    void CommandContext::SetUniformBuffer(std::string_view bindingName,
        render_graph::ResourceID resourceID,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetUniformBuffer(bindingName, resources->GetBuffer(resourceID), offset, range);
    }

    void CommandContext::SetStorageBuffer(uint32_t set,
        uint32_t binding,
        const BufferPtr &buffer,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        Assert(binding < MAX_BINDINGS_PER_DESCRIPTOR_SET, "binding index too high");
        Assert(buffer->IsAcquired(handle), "SetStorageBuffer called without acquiring buffer first");
        auto &bindingDesc = shaderData.sets[set].bindings[binding];
        bindingDesc.uniqueID = buffer ? buffer->GetUniqueID() : 0;

        auto &bufferBinding = bindingDesc.buffer;
        if (buffer) {
            bufferBinding.buffer = **buffer;
            bufferBinding.offset = offset;
            bufferBinding.range = range == 0 ? buffer->ByteSize() - offset : range;
            bindingDesc.arrayStride = buffer->ArrayStride();

            checkBufferOffsets(buffer, bufferBinding.offset, bufferBinding.range);
        } else {
            bufferBinding.buffer = nullptr;
            bufferBinding.offset = 0;
            bufferBinding.range = 0;
            bindingDesc.arrayStride = 0;
        }
        SetDescriptorDirty(set);
    }

    void CommandContext::SetStorageBuffer(std::string_view bindingName,
        const BufferPtr &buffer,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        std::shared_ptr<Shader> lastShader;
        for (auto stage : magic_enum::enum_values<ShaderStage>()) {
            auto &slot = pipelineInput.state.shaders[stage];
            auto shader = device.GetShader(slot);
            if (!shader) continue;
            lastShader = shader;
            for (auto &set : shader->descriptorSets) {
                for (auto &binding : set.bindings) {
                    if (binding.type != vk::DescriptorType::eStorageBuffer) continue;
                    if (binding.name == bindingName) {
                        SetStorageBuffer(set.setId, binding.bindingId, buffer, offset, range);
                        return;
                    }
                }
            }
        }
        Assert(lastShader, "SetStorageBuffer called with no shader bound");
        Errorf("SetStorageBuffer binding %s not found on any bound shader: (last: %s)", bindingName, lastShader->name);
    }

    void CommandContext::SetStorageBuffer(std::string_view bindingName,
        std::string_view resourceName,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetStorageBuffer(bindingName, resources->GetBuffer(resourceName), offset, range);
    }

    void CommandContext::SetStorageBuffer(std::string_view bindingName,
        render_graph::ResourceID resourceID,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetStorageBuffer(bindingName, resources->GetBuffer(resourceID), offset, range);
    }

    void CommandContext::SetBindlessDescriptors(uint32_t set, vk::DescriptorSet descriptorSet) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        bindlessSets[set] = descriptorSet;
        SetDescriptorDirty(set);
    }

    void CommandContext::FlushDescriptorSets(vk::PipelineBindPoint bindPoint) {
        auto layout = currentPipeline->GetLayout();

        for (uint32_t set = 0; set < MAX_BOUND_DESCRIPTOR_SETS; set++) {
            if (!ResetDescriptorDirty(set)) continue;
            if (!layout->HasDescriptorSet(set)) continue;

            vk::DescriptorSet descriptorSet;
            if (layout->IsBindlessSet(set)) {
                descriptorSet = bindlessSets[set];
            } else {
                descriptorSet = layout->GetFilledDescriptorSet(set, shaderData.sets[set]);
            }

            cmd->bindDescriptorSets(bindPoint, *layout, set, {descriptorSet}, nullptr);
        }
    }

    void CommandContext::FlushPushConstants() {
        auto pipelineLayout = currentPipeline->GetLayout();
        auto &layoutInfo = pipelineLayout->Info();

        if (ResetDirty(DirtyFlags::PushConstants)) {
            auto &range = layoutInfo.pushConstantRange;
            if (range.stageFlags) {
                Assert(range.offset == 0, "push constant range must start at 0");
                cmd->pushConstants(*pipelineLayout, range.stageFlags, 0, range.size, shaderData.pushConstants);
            }
        }
    }

    void CommandContext::FlushComputeState() {
        if (ResetDirty(DirtyFlags::Pipeline)) {
            auto pipeline = device.GetPipeline(pipelineInput);
            if (pipeline != currentPipeline) {
                cmd->bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline);
            }
            currentPipeline = pipeline;
        }

        FlushPushConstants();
        FlushDescriptorSets(vk::PipelineBindPoint::eCompute);
    }

    void CommandContext::FlushGraphicsState() {
        if (ResetDirty(DirtyFlags::Pipeline)) {
            auto pipeline = device.GetPipeline(pipelineInput);
            if (pipeline != currentPipeline) {
                cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
            }
            currentPipeline = pipeline;
        }

        if (ResetDirty(DirtyFlags::Viewport)) {
            std::array<vk::Viewport, MAX_VIEWPORTS> vps;

            for (size_t i = 0; i < pipelineInput.state.viewportCount; i++) {
                auto &viewport = viewports[i];

                vps[i] = vk::Viewport{(float)viewport.offset.x,
                    (float)viewport.offset.y,
                    (float)viewport.extent.width,
                    (float)viewport.extent.height,
                    minDepth,
                    maxDepth};

                if (viewportYDirection == YDirection::Up) {
                    // Negative height sets viewport coordinates to OpenGL style (Y up)
                    vps[i].y = framebuffer->Extent().height - vps[i].y;
                    vps[i].height = -vps[i].height;
                }
            }
            cmd->setViewport(0, pipelineInput.state.viewportCount, vps.data());
        }

        if (ResetDirty(DirtyFlags::Scissor)) {
            std::array<vk::Rect2D, MAX_VIEWPORTS> scs;
            for (size_t i = 0; i < pipelineInput.state.scissorCount; i++) {
                scs[i] = scissors[i];
                if (viewportYDirection == YDirection::Up) {
                    scs[i].offset.y = framebuffer->Extent().height - scs[i].offset.y - scs[i].extent.height;
                }
            }
            cmd->setScissor(0, pipelineInput.state.scissorCount, scs.data());
        }

        if (pipelineInput.state.stencilTest && ResetDirty(DirtyFlags::Stencil)) {
            const auto &front = stencilState[0];
            const auto &back = stencilState[1];
            if (front.writeMask == back.writeMask) {
                cmd->setStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, front.writeMask);
            } else {
                cmd->setStencilWriteMask(vk::StencilFaceFlagBits::eFront, front.writeMask);
                cmd->setStencilWriteMask(vk::StencilFaceFlagBits::eBack, back.writeMask);
            }
            if (front.compareMask == back.compareMask) {
                cmd->setStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, front.compareMask);
            } else {
                cmd->setStencilCompareMask(vk::StencilFaceFlagBits::eFront, front.compareMask);
                cmd->setStencilCompareMask(vk::StencilFaceFlagBits::eBack, back.compareMask);
            }
            if (front.reference == back.reference) {
                cmd->setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, front.reference);
            } else {
                cmd->setStencilReference(vk::StencilFaceFlagBits::eFront, front.reference);
                cmd->setStencilReference(vk::StencilFaceFlagBits::eBack, back.reference);
            }
        }

        FlushPushConstants();
        FlushDescriptorSets(vk::PipelineBindPoint::eGraphics);
    }

    const std::shared_ptr<vk::UniqueFence> &CommandContext::Fence() {
        return fence;
    }

    void CommandContext::AcquireImage(const ImagePtr &image, Access nextAccess) {
        DebugZoneScoped;
        Assert(image, "CommandContext::AcquireImage called on null image");

        std::lock_guard l(image->accessMutex);
        Assert(handle.lock().get() == this, "CommandContext handle invalid");
        CommandContextPtr lastCommand = image->lastCommand.lock();
        if (lastCommand && (lastCommand.owner_before(handle) || handle.owner_before(lastCommand))) {
            Assertf(!lastCommand->recording, "Vulkan commands queued out of order");
        }

        uint32_t lastQueueFamilyIndex = image->lastQueueId ? device.QueueFamilyIndex(*image->lastQueueId) : ~0u;
        uint32_t queueFamilyIndex = device.QueueFamilyIndex(queueId);
        if (lastQueueFamilyIndex == queueFamilyIndex || lastQueueFamilyIndex == ~0u) {
            // No queue ownership transfer required
            const AccessInfo &accessInfo = GetAccessInfo(nextAccess);
            ImageBarrier(image, accessInfo.imageLayout, accessInfo.stageMask, accessInfo.accessMask);
        } else {
            ZoneScopedN("TransferQueueFamily");
            const AccessInfo &lastInfo = GetAccessInfo(image->lastAccess);
            auto subCmd = device.GetFencedCommandContext(NewDispatchSource, *image->lastQueueId);
            ImageBarrierInfo options = {};
            options.srcQueueFamilyIndex = lastQueueFamilyIndex;
            options.dstQueueFamilyIndex = queueFamilyIndex;
            subCmd->ImageBarrier(image, image->lastLayout, lastInfo.stageMask, lastInfo.accessMask, options);
            waitSemaphores.emplace_back(subCmd->signalSemaphores.emplace_back(device.GetEmptySemaphore()));
            waitStages.emplace_back(lastInfo.stageMask);
            device.Submit(NewDispatchSource, subCmd);

            // Acquire new queue family
            options.srcQueueFamilyIndex = lastQueueFamilyIndex;
            options.dstQueueFamilyIndex = queueFamilyIndex;
            const AccessInfo &accessInfo = GetAccessInfo(nextAccess);
            ImageBarrier(image, accessInfo.imageLayout, accessInfo.stageMask, accessInfo.accessMask, options);
        }
        image->lastAccess = nextAccess;
        image->lastQueueId = queueId;
        image->lastCommand = handle;
        acquiredImages.emplace_back(image);
    }

    void CommandContext::AcquireBuffer(const BufferPtr &buffer, Access nextAccess) {
        DebugZoneScoped;
        Assert(buffer, "CommandContext::AcquireBuffer called on null buffer");

        std::lock_guard l(buffer->accessMutex);
        Assert(handle.lock().get() == this, "CommandContext handle invalid");
        CommandContextPtr lastCommand = buffer->lastCommand.lock();
        if (lastCommand && (lastCommand.owner_before(handle) || handle.owner_before(lastCommand))) {
            Assertf(!lastCommand->recording, "Vulkan commands queued out of order");
        }

        uint32_t lastQueueFamilyIndex = buffer->lastQueueId ? device.QueueFamilyIndex(*buffer->lastQueueId) : ~0u;
        uint32_t queueFamilyIndex = device.QueueFamilyIndex(queueId);
        if (lastQueueFamilyIndex == queueFamilyIndex || lastQueueFamilyIndex == ~0u) {
            // No queue ownership transfer required
            const AccessInfo &accessInfo = GetAccessInfo(nextAccess);
            BufferBarrier(buffer, accessInfo.stageMask, accessInfo.accessMask);
        } else {
            ZoneScopedN("TransferQueueFamily");
            const AccessInfo &lastInfo = GetAccessInfo(buffer->lastAccess);
            auto subCmd = device.GetFencedCommandContext(NewDispatchSource, *buffer->lastQueueId);
            BufferBarrierInfo options = {};
            options.srcQueueFamilyIndex = lastQueueFamilyIndex;
            options.dstQueueFamilyIndex = queueFamilyIndex;
            subCmd->BufferBarrier(buffer, lastInfo.stageMask, lastInfo.accessMask, options);
            waitSemaphores.emplace_back(subCmd->signalSemaphores.emplace_back(device.GetEmptySemaphore()));
            waitStages.emplace_back(lastInfo.stageMask);
            device.Submit(NewDispatchSource, subCmd);

            // Acquire new queue family
            options.srcQueueFamilyIndex = lastQueueFamilyIndex;
            options.dstQueueFamilyIndex = queueFamilyIndex;
            const AccessInfo &accessInfo = GetAccessInfo(nextAccess);
            BufferBarrier(buffer, accessInfo.stageMask, accessInfo.accessMask, options);
        }
        buffer->lastAccess = nextAccess;
        buffer->lastQueueId = queueId;
        buffer->lastCommand = handle;
        acquiredBuffers.emplace_back(buffer);
    }

    void CommandContext::AcquireBuffer(const SubBufferPtr &buffer, Access nextAccess) {
        Assert(buffer, "CommandContext::AcquireBuffer called on null sub-buffer");
        AcquireBuffer(buffer->parentBuffer, nextAccess);
    }

} // namespace sp::vulkan
