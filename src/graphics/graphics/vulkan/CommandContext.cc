#include "CommandContext.hh"

#include "DeviceContext.hh"

namespace sp::vulkan {
    CommandContext::CommandContext(DeviceContext &device) : device(device) {
        auto buffers = device.AllocateCommandBuffers(1);
        cmd = std::move(buffers[0]);
    }

    void CommandContext::SetDefaultOpaqueState() {
        SetDepthTest(true, true);
        SetStencilTest(false);
        SetBlending(false);
    }

    void CommandContext::BeginRenderPass(const RenderPassInfo &info) {
        Assert(!framebuffer, "render pass already started");

        framebuffer = device.GetFramebuffer(info);
        pipelineState.renderPass = framebuffer->GetRenderPass();

        renderPass = device.GetRenderPass(info);

        scissor = vk::Rect2D{{0, 0}, framebuffer->Extent()};

        // Negative height sets viewport coordinates to OpenGL style
        viewport = vk::Viewport{0.0f,
                                (float)scissor.extent.height,
                                (float)scissor.extent.width,
                                -(float)scissor.extent.height,
                                0.0f,
                                1.0f};

        dirty = ~DirtyFlags();
        currentPipeline = VK_NULL_HANDLE;

        vk::CommandBufferBeginInfo beginInfo;
        cmd->begin(beginInfo);

        vk::ClearValue clearValues[MAX_COLOR_ATTACHMENTS + 1];
        uint32_t clearCount = 0;

        for (uint32 i = 0; i < info.state.colorAttachmentCount; i++) {
            if (info.state.ShouldClear(i)) {
                clearValues[i].color = info.clearColors[i];
                clearCount = i + 1;
            }
        }

        if (info.HasDepthStencil() && info.state.ShouldClear(RenderPassState::DEPTH_STENCIL_INDEX)) {
            clearValues[info.state.colorAttachmentCount].depthStencil = info.clearDepthStencil;
            clearCount = info.state.colorAttachmentCount + 1;
        }

        vk::RenderPassBeginInfo renderPassBeginInfo;
        renderPassBeginInfo.renderPass = **renderPass;
        renderPassBeginInfo.framebuffer = **framebuffer;
        renderPassBeginInfo.renderArea = scissor;
        renderPassBeginInfo.clearValueCount = clearCount;
        renderPassBeginInfo.pClearValues = clearValues;
        cmd->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    }

    void CommandContext::EndRenderPass() {
        Assert(!!framebuffer, "render pass not started");

        cmd->endRenderPass();
        cmd->end();

        pipelineState.renderPass = VK_NULL_HANDLE;
        framebuffer.reset();
        renderPass.reset();
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

    void CommandContext::SetShader(ShaderStage stage, ShaderHandle handle) {
        auto &slot = pipelineState.state.values.shaders[(size_t)stage];
        if (slot == handle) return;
        slot = handle;
        SetDirty(DirtyBits::Pipeline);
    }

    void CommandContext::SetShader(ShaderStage stage, string name) {
        SetShader(stage, device.LoadShader(name));
    }

    void CommandContext::PushConstants(const void *data, VkDeviceSize offset, VkDeviceSize range) {
        Assert(offset + range <= sizeof(shaderData.pushConstants), "CommandContext::PushConstants overflow");
        memcpy(shaderData.pushConstants + offset, data, range);
        SetDirty(DirtyBits::PushConstants);
    }

    void CommandContext::FlushGraphicsState() {
        if (ResetDirty(DirtyBits::Pipeline)) {
            auto pipeline = device.GetGraphicsPipeline(pipelineState);
            if (pipeline != currentPipeline) { cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, **pipeline); }
            currentPipeline = pipeline;
        }

        auto pipelineLayout = currentPipeline->GetLayout();

        if (ResetDirty(DirtyBits::PushConstants)) {
            auto &range = pipelineLayout->pushConstantRange;
            if (range.stageFlags) {
                Assert(range.offset == 0, "push constant range must start at 0");
                cmd->pushConstants(**pipelineLayout, range.stageFlags, 0, range.size, shaderData.pushConstants);
            }
        }

        if (ResetDirty(DirtyBits::Viewport)) { cmd->setViewport(0, 1, &viewport); }

        if (ResetDirty(DirtyBits::Scissor)) { cmd->setScissor(0, 1, &scissor); }
    }
} // namespace sp::vulkan
