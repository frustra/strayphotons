#include "RenderPass.hh"

#include "DeviceContext.hh"

namespace sp::vulkan {
    RenderPass::RenderPass(DeviceContext &device, const RenderPassInfo &info) {
        uint32 attachmentCount = 0;
        vk::AttachmentDescription attachments[MAX_COLOR_ATTACHMENTS + 1] = {};
        vk::AttachmentReference colorAttachmentRefs[MAX_COLOR_ATTACHMENTS];
        vk::AttachmentReference depthAttachmentRef;

        auto &state = info.state;
        Assert(state.colorAttachmentCount < MAX_COLOR_ATTACHMENTS, "too many attachments");

        for (uint32 i = 0; i < state.colorAttachmentCount; i++) {
            vk::AttachmentDescription &colorAttachment = attachments[i];
            colorAttachment.samples = vk::SampleCountFlagBits::e1;
            colorAttachment.format = state.colorFormats[i];

            colorAttachment.loadOp = state.LoadOp(i);
            colorAttachment.storeOp = state.StoreOp(i);
            colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
            colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

            colorAttachment.finalLayout = vk::ImageLayout::eUndefined;

            if (info.colorAttachments[i]->IsSwapchain()) {
                if (colorAttachment.loadOp == vk::AttachmentLoadOp::eLoad) {
                    colorAttachment.initialLayout = info.colorAttachments[i]->SwapchainLayout();
                } else {
                    colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
                }
                colorAttachment.finalLayout = info.colorAttachments[i]->SwapchainLayout();
            } else {
                colorAttachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
            }

            vk::AttachmentReference &colorAttachmentRef = colorAttachmentRefs[i];
            colorAttachmentRef.attachment = i;
            colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

            attachmentCount++;
        }

        if (state.HasDepthStencil()) {
            vk::AttachmentDescription &depthStencilAttachment = attachments[attachmentCount];
            depthStencilAttachment.samples = vk::SampleCountFlagBits::e1;
            depthStencilAttachment.format = state.depthStencilFormat;

            depthStencilAttachment.loadOp = state.LoadOp(RenderPassState::DEPTH_STENCIL_INDEX);
            depthStencilAttachment.storeOp = state.StoreOp(RenderPassState::DEPTH_STENCIL_INDEX);

            if (FormatToAspectFlags(depthStencilAttachment.format) & vk::ImageAspectFlagBits::eStencil) {
                depthStencilAttachment.stencilLoadOp = depthStencilAttachment.loadOp;
                depthStencilAttachment.stencilStoreOp = depthStencilAttachment.storeOp;
            } else {
                depthStencilAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
                depthStencilAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
            }

            depthStencilAttachment.initialLayout = vk::ImageLayout::eUndefined;
            depthStencilAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            depthAttachmentRef.attachment = attachmentCount;
            depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

            attachmentCount++;
        }

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = state.colorAttachmentCount;
        subpass.pColorAttachments = colorAttachmentRefs;
        subpass.pDepthStencilAttachment = nullptr;

        if (state.HasDepthStencil()) { subpass.pDepthStencilAttachment = &depthAttachmentRef; }

        // TODO: this external dependency is specifically for the swapchain renderpass, make it configurable
        vk::SubpassDependency dependency;
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                  vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                  vk::PipelineStageFlagBits::eEarlyFragmentTests;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                   vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = attachmentCount;
        renderPassInfo.pAttachments = attachments;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        uniqueHandle = device->createRenderPassUnique(renderPassInfo);
    }

    Framebuffer::Framebuffer(DeviceContext &device, const RenderPassInfo &info) {
        renderPass = device.GetRenderPass(info);

        extent = vk::Extent2D{UINT32_MAX, UINT32_MAX};
        vk::ImageView attachments[MAX_COLOR_ATTACHMENTS + 1];
        uint32 attachmentCount = info.state.colorAttachmentCount;

        for (uint32 i = 0; i < info.state.colorAttachmentCount; i++) {
            auto &image = *info.colorAttachments[i];
            attachments[i] = image;
            extent.width = std::min(extent.width, image.Extent().width);
            extent.height = std::min(extent.width, image.Extent().height);
        }

        if (info.HasDepthStencil()) {
            attachmentCount++;
            auto &image = *info.depthStencilAttachment;
            attachments[info.state.colorAttachmentCount] = image;
            extent.width = std::min(extent.width, image.Extent().width);
            extent.height = std::min(extent.width, image.Extent().height);
        }

        vk::FramebufferCreateInfo framebufferInfo;
        framebufferInfo.renderPass = *renderPass;
        framebufferInfo.attachmentCount = attachmentCount;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        uniqueHandle = device->createFramebufferUnique(framebufferInfo);
    }

    RenderPassManager::RenderPassManager(DeviceContext &device) : device(device) {}

    shared_ptr<RenderPass> RenderPassManager::GetRenderPass(const RenderPassInfo &info) {
        RenderPassKey key(info.state);
        auto &pass = renderPasses[key];
        if (!pass) pass = make_shared<RenderPass>(device, info);
        return pass;
    }

    FramebufferManager::FramebufferManager(DeviceContext &device) : device(device) {}

    shared_ptr<Framebuffer> FramebufferManager::GetFramebuffer(const RenderPassInfo &info) {
        FramebufferKey key;
        key.input.renderPass = info.state;

        for (uint32 i = 0; i < info.state.colorAttachmentCount; i++) {
            auto image = info.colorAttachments[i];
            Assert(!!image, "render pass is missing a color image");
            key.input.imageViews[i] = *image;

            auto extent = image->Extent();
            key.input.extents[i].width = extent.width;
            key.input.extents[i].height = extent.height;
        }

        if (info.HasDepthStencil()) {
            auto image = info.depthStencilAttachment;
            Assert(!!image, "render pass is missing depth image");
            key.input.imageViews[MAX_COLOR_ATTACHMENTS] = *image;
        }

        auto &fb = framebuffers[key];
        if (!fb) fb = make_shared<Framebuffer>(device, info);
        return fb;
    }
} // namespace sp::vulkan
