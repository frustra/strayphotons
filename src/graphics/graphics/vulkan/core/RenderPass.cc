/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "RenderPass.hh"

#include "common/Logging.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    RenderPass::RenderPass(DeviceContext &device, const RenderPassInfo &info) : state(info.state) {
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
                colorAttachment.finalLayout = vk::ImageLayout::eColorAttachmentOptimal;
            }

            vk::AttachmentReference &colorAttachmentRef = colorAttachmentRefs[i];
            colorAttachmentRef.attachment = i;
            colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

            initialLayouts[i] = colorAttachment.initialLayout;
            finalLayouts[i] = colorAttachment.finalLayout;
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

            if (depthStencilAttachment.loadOp == vk::AttachmentLoadOp::eLoad) {
                if (depthStencilAttachment.storeOp == vk::AttachmentStoreOp::eDontCare ||
                    state.ReadOnly(RenderPassState::DEPTH_STENCIL_INDEX)) {
                    depthStencilAttachment.initialLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
                } else {
                    depthStencilAttachment.initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
                }
            } else {
                depthStencilAttachment.initialLayout = vk::ImageLayout::eUndefined;
            }

            if (depthStencilAttachment.storeOp == vk::AttachmentStoreOp::eDontCare ||
                state.ReadOnly(RenderPassState::DEPTH_STENCIL_INDEX)) {
                depthStencilAttachment.finalLayout = depthStencilAttachment.initialLayout;
            } else {
                depthStencilAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
            }

            depthAttachmentRef.attachment = attachmentCount;
            depthAttachmentRef.layout = depthStencilAttachment.finalLayout;

            initialLayouts[attachmentCount] = depthStencilAttachment.initialLayout;
            finalLayouts[attachmentCount] = depthStencilAttachment.finalLayout;
            attachmentCount++;
        }

        vk::SubpassDescription subpass;
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
        subpass.colorAttachmentCount = state.colorAttachmentCount;
        subpass.pColorAttachments = colorAttachmentRefs;
        subpass.pDepthStencilAttachment = nullptr;

        if (state.HasDepthStencil()) {
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
        }

        vk::RenderPassCreateInfo renderPassInfo;
        renderPassInfo.attachmentCount = attachmentCount;
        renderPassInfo.pAttachments = attachments;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 0;

        vk::SubpassDependency dependency;
        if (state.colorAttachmentCount > 0 && info.colorAttachments[0]->IsSwapchain()) {
            // TODO: this external dependency is specifically for the swapchain renderpass, make it configurable
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.dstSubpass = 0;
            dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                      vk::PipelineStageFlagBits::eEarlyFragmentTests;
            dependency.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
            dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput |
                                      vk::PipelineStageFlagBits::eEarlyFragmentTests;
            dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                       vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            renderPassInfo.pDependencies = &dependency;
            renderPassInfo.dependencyCount = 1;
        }

        vk::RenderPassMultiviewCreateInfo multiviewInfo;
        if (state.multiviewMask) {
            multiviewInfo.pViewMasks = &state.multiviewMask;
            multiviewInfo.pCorrelationMasks = &state.multiviewCorrelationMask;
            multiviewInfo.subpassCount = renderPassInfo.subpassCount;
            Assert(multiviewInfo.subpassCount == 1, "need to update this code, pViewMasks needs to be an array");
            renderPassInfo.pNext = &multiviewInfo;
        }

        uniqueHandle = device->createRenderPassUnique(renderPassInfo);
    }

    void RenderPass::RecordImplicitImageLayoutTransitions(const RenderPassInfo &info) {
        for (uint32 i = 0; i < info.state.colorAttachmentCount; i++) {
            info.colorAttachments[i]->Image()->SetLayout(initialLayouts[i], finalLayouts[i]);
        }
        if (info.HasDepthStencil()) {
            auto i = info.state.colorAttachmentCount;
            info.depthStencilAttachment->Image()->SetLayout(initialLayouts[i], finalLayouts[i]);
        }
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
            extent.height = std::min(extent.height, image.Extent().height);
        }

        if (info.HasDepthStencil()) {
            attachmentCount++;
            auto &image = *info.depthStencilAttachment;
            attachments[info.state.colorAttachmentCount] = image;
            extent.width = std::min(extent.width, image.Extent().width);
            extent.height = std::min(extent.height, image.Extent().height);
        }

        if (extent.width == UINT32_MAX) extent.width = 1;
        if (extent.height == UINT32_MAX) extent.height = 1;

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
            auto imageView = info.colorAttachments[i];
            Assert(!!imageView, "render pass is missing a color image");
            key.input.imageViewIDs[i] = imageView->GetUniqueID();

            auto extent = imageView->Extent();
            key.input.extents[i].width = extent.width;
            key.input.extents[i].height = extent.height;
        }

        if (info.HasDepthStencil()) {
            auto image = info.depthStencilAttachment;
            Assert(!!image, "render pass is missing depth image");
            key.input.imageViewIDs[MAX_COLOR_ATTACHMENTS] = image->GetUniqueID();
        }

        auto &fb = framebuffers[key];
        if (!fb) fb = make_shared<Framebuffer>(device, info);
        return fb;
    }
} // namespace sp::vulkan
