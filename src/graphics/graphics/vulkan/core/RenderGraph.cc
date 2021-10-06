#include "RenderGraph.hh"

#include "core/Logging.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    void RenderGraphResources::ResizeBeforeExecute() {
        refCounts.resize(resources.size());
        renderTargets.resize(resources.size());
    }

    RenderTargetPtr RenderGraphResources::GetRenderTarget(string_view name) {
        return GetRenderTarget(GetID(name));
    }

    RenderTargetPtr RenderGraphResources::GetRenderTarget(RenderGraphResourceID id) {
        if (id >= resources.size()) return nullptr;
        auto &res = resources[id];
        Assert(res.type == RenderGraphResource::Type::RenderTarget, "resources is not a render target");
        auto &target = renderTargets[res.id];
        if (!target) target = device.GetRenderTarget(res.renderTargetDesc);
        return target;
    }

    uint32 RenderGraphResources::RefCount(RenderGraphResourceID id) {
        Assert(id < resources.size(), "id out of range");
        return refCounts[id];
    }

    void RenderGraphResources::IncrementRef(RenderGraphResourceID id) {
        Assert(id < resources.size(), "id out of range");
        ++refCounts[id];
    }

    void RenderGraphResources::DecrementRef(RenderGraphResourceID id) {
        Assert(id < resources.size(), "id out of range");
        if (--refCounts[id] > 0) return;

        auto &res = resources[id];
        switch (res.type) {
        case RenderGraphResource::Type::RenderTarget:
            renderTargets[id].reset();
            break;
        default:
            Abort("resource type is undefined");
        }
    }

    const RenderGraphResource &RenderGraphResources::GetResourceByName(string_view name) const {
        return GetResourceByID(GetID(name));
    }

    const RenderGraphResource &RenderGraphResources::GetResourceByID(RenderGraphResourceID id) const {
        if (id < resources.size()) return resources[id];
        static RenderGraphResource invalidResource = {};
        return invalidResource;
    }

    void RenderGraph::SetTargetImageView(string_view name, ImageViewPtr view) {
        auto &res = resources.GetResourceByName(name);
        Assert(res.renderTargetDesc.extent == view->Extent(), "image extent mismatch");

        auto resFormat = res.renderTargetDesc.format;
        auto viewFormat = view->Format();
        Assert(FormatComponentCount(resFormat) == FormatComponentCount(viewFormat), "image component count mismatch");
        Assert(FormatByteSize(resFormat) == FormatByteSize(viewFormat), "image component size mismatch");

        Assert(view->BaseArrayLayer() == 0, "view can't target a specific layer");
        Assert(res.renderTargetDesc.arrayLayers == view->ArrayLayers(), "image array mismatch");

        resources.ResizeBeforeExecute();
        resources.renderTargets[res.id] = make_shared<RenderTarget>(res.renderTargetDesc, view, ~0u);
    }

    void RenderGraph::Execute() {
        resources.ResizeBeforeExecute();

        // passes are already sorted by dependency order
        for (auto it = passes.rbegin(); it != passes.rend(); it++) {
            auto &pass = *it;
            pass.active = pass.required;
            if (!pass.active) {
                for (auto &out : pass.outputs) {
                    if (resources.RefCount(out) > 0) {
                        pass.active = true;
                        break;
                    }
                }
            }
            if (!pass.active) continue;
            for (auto &dep : pass.dependencies) {
                resources.IncrementRef(dep.id);
            }
        }

        for (auto &pass : passes) {
            if (!pass.active) continue;

            auto cmd = device.GetCommandContext();
            AddPassBarriers(*cmd, pass);

            bool isRenderPass = false;
            RenderPassInfo renderPassInfo;

            for (uint32 i = 0; i < pass.attachments.size(); i++) {
                auto &attachment = pass.attachments[i];
                if (attachment.resourceID == ~0u) continue;
                isRenderPass = true;

                auto imageView = resources.GetRenderTarget(attachment.resourceID)->ImageView();

                if (i != MAX_COLOR_ATTACHMENTS) {
                    renderPassInfo.state.colorAttachmentCount = i + 1;
                    renderPassInfo.SetColorAttachment(i,
                                                      imageView,
                                                      attachment.loadOp,
                                                      attachment.storeOp,
                                                      attachment.clearColor);
                } else {
                    renderPassInfo.SetDepthStencilAttachment(imageView,
                                                             attachment.loadOp,
                                                             attachment.storeOp,
                                                             attachment.clearDepthStencil);
                }
            }

            if (isRenderPass) cmd->BeginRenderPass(renderPassInfo);
            pass.Execute(resources, *cmd);
            if (isRenderPass) cmd->EndRenderPass();
            device.Submit(cmd);

            for (auto &dep : pass.dependencies) {
                resources.DecrementRef(dep.id);
            }
        }
        passes.clear();
    }

    void RenderGraph::AddPassBarriers(CommandContext &cmd, RenderGraphPass &pass) {
        for (auto &dep : pass.dependencies) {
            if (dep.access.layout == vk::ImageLayout::eUndefined) continue;

            auto &res = resources.resources[dep.id];
            Assert(res.type == RenderGraphResource::Type::RenderTarget, "resource type must be RenderTarget");

            auto image = resources.GetRenderTarget(dep.id)->ImageView()->Image();
            cmd.ImageBarrier(image,
                             image->LastLayout(),
                             dep.access.layout,
                             vk::PipelineStageFlagBits::eColorAttachmentOutput, // TODO: infer these
                             vk::AccessFlagBits::eColorAttachmentWrite,
                             dep.access.stages,
                             dep.access.access);
        }

        for (auto id : pass.outputs) {
            // assume pass doesn't depend on one of its outputs (e.g. blending), TODO: implement
            auto &res = resources.resources[id];

            Assert(res.type == RenderGraphResource::Type::RenderTarget, "resource type must be RenderTarget");

            auto view = resources.GetRenderTarget(id)->ImageView();
            auto image = view->Image();
            if (view->IsSwapchain()) continue; // barrier handled by RenderPass implicitly

            if (res.renderTargetDesc.usage & vk::ImageUsageFlagBits::eColorAttachment) {
                if (image->LastLayout() == vk::ImageLayout::eColorAttachmentOptimal) continue;
                cmd.ImageBarrier(image,
                                 vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eColorAttachmentOptimal,
                                 vk::PipelineStageFlagBits::eBottomOfPipe,
                                 {},
                                 vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                 vk::AccessFlagBits::eColorAttachmentWrite);
            } else if (res.renderTargetDesc.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) {
                if (image->LastLayout() == vk::ImageLayout::eDepthAttachmentOptimal) continue;
                cmd.ImageBarrier(image,
                                 vk::ImageLayout::eUndefined,
                                 vk::ImageLayout::eDepthAttachmentOptimal,
                                 vk::PipelineStageFlagBits::eBottomOfPipe,
                                 {},
                                 vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                 vk::AccessFlagBits::eDepthStencilAttachmentWrite);
            }
        }
    }
} // namespace sp::vulkan
