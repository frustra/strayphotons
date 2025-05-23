/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "CommandContext.hh"

#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/render_graph/Resources.hh"

namespace sp::vulkan {
    CommandContext::CommandContext(DeviceContext &device,
        vk::UniqueCommandBuffer cmd,
        CommandContextType type,
        CommandContextScope scope) noexcept
        : device(device), cmd(std::move(cmd)), type(type), scope(scope) {
        SetDefaultOpaqueState();
    }

    CommandContext::~CommandContext() {
        Assert(!recording, "command context was never ended/submitted");
    }

    void CommandContext::SetDefaultOpaqueState() {
        SetDepthTest(true, true);
        SetDepthRange(0.0f, 1.0f);
        SetStencilTest(false);
        SetBlending(false);
        SetBlendFunc(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha);
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
        uint32 clearValueCount = info.state.colorAttachmentCount;

        for (uint32 i = 0; i < info.state.colorAttachmentCount; i++) {
            if (info.state.ShouldClear(i)) {
                clearValues[i].color = info.clearColors[i];
            }
            if (info.colorAttachments[i]->IsSwapchain()) writesToSwapchain = true;
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
        Assert(!recording, "command buffer already recording");
        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd->begin(beginInfo);
        recording = true;
        this->resources = resources;
    }

    void CommandContext::End() {
        Assert(recording, "command buffer not recording");
        cmd->end();
        recording = false;
    }

    void CommandContext::Abandon() {
        if (recording) {
            cmd->end();
            recording = false;
            abandoned = true;
        }
    }

    void CommandContext::Dispatch(uint32 groupCountX, uint32 groupCountY, uint32 groupCountZ) {
        FlushComputeState();
        cmd->dispatch(groupCountX, groupCountY, groupCountZ);
    }

    void CommandContext::DispatchIndirect(BufferPtr indirectBuffer, vk::DeviceSize offset) {
        FlushComputeState();
        cmd->dispatchIndirect(*indirectBuffer, offset);
    }

    void CommandContext::Draw(uint32 vertexes, uint32 instances, int32 firstVertex, uint32 firstInstance) {
        FlushGraphicsState();
        cmd->draw(vertexes, instances, firstVertex, firstInstance);
    }

    void CommandContext::DrawIndexed(uint32 indexes,
        uint32 instances,
        uint32 firstIndex,
        int32 vertexOffset,
        uint32 firstInstance) {
        FlushGraphicsState();
        cmd->drawIndexed(indexes, instances, firstIndex, vertexOffset, firstInstance);
    }

    void CommandContext::DrawIndirect(BufferPtr drawCommands, vk::DeviceSize offset, uint32 drawCount, uint32 stride) {
        FlushGraphicsState();
        cmd->drawIndirect(*drawCommands, offset, drawCount, stride);
    }

    void CommandContext::DrawIndirectCount(BufferPtr drawCommands,
        vk::DeviceSize offset,
        BufferPtr countBuffer,
        vk::DeviceSize countOffset,
        uint32 maxDrawCount,
        uint32 stride) {
        FlushGraphicsState();
        cmd->drawIndirectCount(*drawCommands, offset, *countBuffer, countOffset, maxDrawCount, stride);
    }

    void CommandContext::DrawIndexedIndirect(BufferPtr drawCommands,
        vk::DeviceSize offset,
        uint32 drawCount,
        uint32 stride) {
        FlushGraphicsState();
        cmd->drawIndexedIndirect(*drawCommands, offset, drawCount, stride);
    }

    void CommandContext::DrawIndexedIndirectCount(BufferPtr drawCommands,
        vk::DeviceSize offset,
        BufferPtr countBuffer,
        vk::DeviceSize countOffset,
        uint32 maxDrawCount,
        uint32 stride) {
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
            image->SetLayout(oldLayout, newLayout);
        }
    }

    void CommandContext::SetShaders(std::initializer_list<std::pair<ShaderStage, string_view>> shaders) {
        pipelineInput.state.shaders = {};
        for (auto &s : shaders) {
            SetSingleShader(s.first, s.second);
        }
    }

    void CommandContext::SetShaders(string_view vertName, string_view fragName) {
        pipelineInput.state.shaders = {};
        SetSingleShader(ShaderStage::Vertex, vertName);
        SetSingleShader(ShaderStage::Fragment, fragName);
    }

    void CommandContext::SetComputeShader(string_view name) {
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

    void CommandContext::SetSingleShader(ShaderStage stage, string_view name) {
        SetSingleShader(stage, device.LoadShader(name));
    }

    void CommandContext::SetShaderConstant(ShaderStage stage, uint32 index, uint32 data) {
        Assert(pipelineInput.state.shaders[stage], "no shader bound to set constant");
        auto &spec = pipelineInput.state.specializations[stage];
        spec.values[index] = data;
        spec.set.set(index, true);
        SetDirty(DirtyFlags::Pipeline);
    }

    void CommandContext::SetShaderConstant(ShaderStage stage, string_view name, uint32 data) {
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

    void CommandContext::SetSampler(uint32 set, uint32 binding, const vk::Sampler &sampler) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        Assert(binding < MAX_BINDINGS_PER_DESCRIPTOR_SET, "binding index too high");
        auto &image = shaderData.sets[set].bindings[binding].image;
        image.sampler = sampler;
        SetDescriptorDirty(set);
    }

    void CommandContext::SetSampler(string_view bindingName, const vk::Sampler &sampler) {
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

    void CommandContext::SetImageView(uint32 set, uint32 binding, const ImageViewPtr &view) {
        SetImageView(set, binding, view.get());
    }

    void CommandContext::SetImageView(uint32 set, uint32 binding, const ImageView *view) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        Assert(binding < MAX_BINDINGS_PER_DESCRIPTOR_SET, "binding index too high");
        auto &bindingDesc = shaderData.sets[set].bindings[binding];
        bindingDesc.uniqueID = view->GetUniqueID();

        auto &image = bindingDesc.image;
        image.imageView = **view;
        image.imageLayout = (VkImageLayout)view->Image()->LastLayout();
        SetDescriptorDirty(set);

        auto defaultSampler = view->DefaultSampler();
        if (defaultSampler) SetSampler(set, binding, defaultSampler);
    }

    void CommandContext::SetImageView(string_view bindingName, const ImageViewPtr &view) {
        SetImageView(bindingName, view.get());
    }

    void CommandContext::SetImageView(string_view bindingName, const ImageView *view) {
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

    void CommandContext::SetImageView(string_view bindingName, render_graph::ResourceID resourceID) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetImageView(bindingName, resources->GetImageView(resourceID));
    }

    void CommandContext::SetImageView(string_view bindingName, string_view resourceName) {
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

    void CommandContext::SetUniformBuffer(uint32 set,
        uint32 binding,
        const BufferPtr &buffer,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        Assert(binding < MAX_BINDINGS_PER_DESCRIPTOR_SET, "binding index too high");
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

    void CommandContext::SetUniformBuffer(string_view bindingName,
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

    void CommandContext::SetUniformBuffer(string_view bindingName,
        string_view resourceName,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetUniformBuffer(bindingName, resources->GetBuffer(resourceName), offset, range);
    }

    void CommandContext::SetUniformBuffer(string_view bindingName,
        render_graph::ResourceID resourceID,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetUniformBuffer(bindingName, resources->GetBuffer(resourceID), offset, range);
    }

    void CommandContext::SetStorageBuffer(uint32 set,
        uint32 binding,
        const BufferPtr &buffer,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        Assert(binding < MAX_BINDINGS_PER_DESCRIPTOR_SET, "binding index too high");
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

    void CommandContext::SetStorageBuffer(string_view bindingName,
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

    void CommandContext::SetStorageBuffer(string_view bindingName,
        string_view resourceName,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetStorageBuffer(bindingName, resources->GetBuffer(resourceName), offset, range);
    }

    void CommandContext::SetStorageBuffer(string_view bindingName,
        render_graph::ResourceID resourceID,
        vk::DeviceSize offset,
        vk::DeviceSize range) {
        Assert(resources, "Render Graph resources not set on CommandContext");
        SetStorageBuffer(bindingName, resources->GetBuffer(resourceID), offset, range);
    }

    BufferPtr CommandContext::AllocUniformBuffer(uint32 set, uint32 binding, vk::DeviceSize size) {
        BufferDesc desc;
        desc.layout = size;
        desc.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        desc.residency = Residency::CPU_TO_GPU;
        auto buffer = device.GetBuffer(desc);
        SetUniformBuffer(set, binding, buffer);
        return buffer;
    }

    BufferPtr CommandContext::AllocUniformBuffer(string_view bindingName, vk::DeviceSize size) {
        BufferDesc desc;
        desc.layout = size;
        desc.usage = vk::BufferUsageFlagBits::eUniformBuffer;
        desc.residency = Residency::CPU_TO_GPU;
        auto buffer = device.GetBuffer(desc);
        SetUniformBuffer(bindingName, buffer);
        return buffer;
    }

    void CommandContext::SetBindlessDescriptors(uint32 set, vk::DescriptorSet descriptorSet) {
        Assert(set < MAX_BOUND_DESCRIPTOR_SETS, "descriptor set index too high");
        bindlessSets[set] = descriptorSet;
        SetDescriptorDirty(set);
    }

    void CommandContext::FlushDescriptorSets(vk::PipelineBindPoint bindPoint) {
        auto layout = currentPipeline->GetLayout();

        for (uint32 set = 0; set < MAX_BOUND_DESCRIPTOR_SETS; set++) {
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
                scs[i].offset.y = framebuffer->Extent().height - scs[i].offset.y - scs[i].extent.height;
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

    vk::Fence CommandContext::Fence() {
        if (abandoned) return {};

        if (!fence && scope == CommandContextScope::Fence) {
            vk::FenceCreateInfo fenceInfo;
            fence = device->createFenceUnique(fenceInfo);
        }
        return *fence;
    }

} // namespace sp::vulkan
