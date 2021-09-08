#include "CommandContext.hh"

#include "DeviceContext.hh"

namespace sp::vulkan {
    CommandContext::CommandContext(DeviceContext &device, vk::UniqueCommandBuffer cmd, CommandContextType type) noexcept
        : device(device), cmd(std::move(cmd)), type(type) {}

    CommandContext::~CommandContext() {
        Assert(!recording, "dangling command context");
    }

    void CommandContext::SetDefaultOpaqueState() {
        SetDepthTest(true, true);
        SetDepthRange(0.0f, 1.0f);
        SetStencilTest(false);
        SetBlending(false);
        SetBlendFunc(vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha);
        SetCullMode(vk::CullModeFlagBits::eBack);
    }

    void CommandContext::BeginRenderPass(const RenderPassInfo &info) {
        Assert(!framebuffer, "render pass already started");

        framebuffer = device.GetFramebuffer(info);
        pipelineInput.renderPass = framebuffer->GetRenderPass();

        renderPass = device.GetRenderPass(info);

        viewport = vk::Rect2D{{0, 0}, framebuffer->Extent()};
        scissor = viewport;

        dirty = ~DirtyFlags();
        dirtyDescriptorSets = ~0u;
        currentPipeline = VK_NULL_HANDLE;

        vk::ClearValue clearValues[MAX_COLOR_ATTACHMENTS + 1];
        size_t clearCount = 0;

        for (uint32 i = 0; i < info.state.colorAttachmentCount; i++) {
            if (info.state.ShouldClear(i)) {
                clearValues[i].color = info.clearColors[i];
                clearCount = i + 1;
            }
            if (info.colorAttachments[i].IsSwapchain()) writesToSwapchain = true;
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

        pipelineInput.renderPass = VK_NULL_HANDLE;
        framebuffer.reset();
        renderPass.reset();
    }

    void CommandContext::Begin() {
        Assert(!recording, "command buffer already recording");
        vk::CommandBufferBeginInfo beginInfo;
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmd->begin(beginInfo);
        recording = true;
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

    void CommandContext::SetShaders(const string &vertName, const string &fragName) {
        SetShader(ShaderStage::Vertex, vertName);
        SetShader(ShaderStage::Fragment, fragName);
        SetShader(ShaderStage::Geometry, 0);
        SetShader(ShaderStage::Compute, 0);
    }

    void CommandContext::SetShader(ShaderStage stage, ShaderHandle handle) {
        auto &slot = pipelineInput.state.shaders[(size_t)stage];
        if (slot == handle) return;
        slot = handle;
        SetDirty(DirtyBits::Pipeline);
    }

    void CommandContext::SetShader(ShaderStage stage, const string &name) {
        SetShader(stage, device.LoadShader(name));
    }

    void CommandContext::PushConstants(const void *data, VkDeviceSize offset, VkDeviceSize range) {
        Assert(offset + range <= sizeof(shaderData.pushConstants), "CommandContext::PushConstants overflow");
        memcpy(shaderData.pushConstants + offset, data, range);
        SetDirty(DirtyBits::PushConstants);
    }

    void CommandContext::FlushDescriptorSets() {
        auto layout = currentPipeline->GetLayout();

        for (uint32 set = 0; set < MAX_BOUND_DESCRIPTOR_SETS; set++) {
            if (!ResetDescriptorDirty(set)) continue;
            if (!layout->HasDescriptorSet(set)) continue;

            auto descriptorSet = layout->GetFilledDescriptorSet(set, shaderData.sets[set]);
            cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, **layout, set, {descriptorSet}, nullptr);
        }
    }

    void CommandContext::FlushGraphicsState() {
        if (ResetDirty(DirtyBits::Pipeline)) {
            auto pipeline = device.GetGraphicsPipeline(pipelineInput);
            if (pipeline != currentPipeline) { cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, **pipeline); }
            currentPipeline = pipeline;
        }

        auto pipelineLayout = currentPipeline->GetLayout();
        auto &layoutInfo = pipelineLayout->Info();

        if (ResetDirty(DirtyBits::PushConstants)) {
            auto &range = layoutInfo.pushConstantRange;
            if (range.stageFlags) {
                Assert(range.offset == 0, "push constant range must start at 0");
                cmd->pushConstants(**pipelineLayout, range.stageFlags, 0, range.size, shaderData.pushConstants);
            }
        }

        if (ResetDirty(DirtyBits::Viewport)) {
            // Negative height sets viewport coordinates to OpenGL style (Y up)
            vk::Viewport vp = {(float)viewport.offset.x,
                               (float)(framebuffer->Extent().height - viewport.offset.y),
                               (float)viewport.extent.width,
                               -(float)viewport.extent.height,
                               minDepth,
                               maxDepth};
            cmd->setViewport(0, 1, &vp);
        }

        if (ResetDirty(DirtyBits::Scissor)) {
            vk::Rect2D sc = scissor;
            sc.offset.y = framebuffer->Extent().height - sc.offset.y - sc.extent.height;
            cmd->setScissor(0, 1, &sc);
        }

        FlushDescriptorSets();
    }
} // namespace sp::vulkan
