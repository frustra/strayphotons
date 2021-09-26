#include "RenderGraph.hh"

#include "DeviceContext.hh"
#include "core/Logging.hh"

namespace sp::vulkan {
    void RenderGraphResources::ResizeBeforeExecute() {
        refCounts.resize(resources.size());
        renderTargets.resize(resources.size());
    }

    RenderTargetPtr RenderGraphResources::GetRenderTarget(const RenderGraphResource &res) {
        Assert(res.type == RenderGraphResource::Type::RenderTarget, "resources is not a render target");
        auto &target = renderTargets[res.id];
        if (!target) target = device.GetRenderTarget(res.renderTargetDesc);
        return target;
    }

    void RenderGraphResources::IncrementRef(uint32 id) {
        Assert(id < resources.size(), "id out of range");
        ++refCounts[id];
    }

    void RenderGraphResources::DecrementRef(uint32 id) {
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
        for (size_t i = 0; i < names.size(); i++) {
            if (names[i] == name) return resources[i];
        }
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
        Assert(res.renderTargetDesc.arrayLayers == view->Image()->ArrayLayers(), "image array mismatch");

        resources.ResizeBeforeExecute();
        resources.renderTargets[res.id] = make_shared<RenderTarget>(res.renderTargetDesc, view, ~0u);
    }

    void RenderGraph::Execute() {
        resources.ResizeBeforeExecute();

        for (auto &pass : passes) {
            for (auto id : pass->dependencies) {
                resources.IncrementRef(id);
            }
        }

        for (auto &pass : passes) {
            auto cmd = device.GetCommandContext();
            pass->Execute(resources, *cmd);
            device.Submit(cmd);

            for (auto id : pass->dependencies) {
                resources.DecrementRef(id);
            }
        }
        passes.clear();
    }
} // namespace sp::vulkan