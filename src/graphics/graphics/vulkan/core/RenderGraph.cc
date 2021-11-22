#include "RenderGraph.hh"

#include "core/Logging.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"

namespace sp::vulkan {
    void RenderGraphResources::ResizeBeforeExecute() {
        refCounts.resize(resources.size());
        renderTargets.resize(resources.size());
        buffers.resize(resources.size());
    }

    RenderTargetPtr RenderGraphResources::GetRenderTarget(string_view name) {
        return GetRenderTarget(GetID(name));
    }

    RenderTargetPtr RenderGraphResources::GetRenderTarget(RenderGraphResourceID id) {
        if (id >= resources.size()) return nullptr;
        auto &res = resources[id];
        Assert(res.type == RenderGraphResource::Type::RenderTarget, "resource is not a render target");
        auto &target = renderTargets[res.id];
        if (!target) target = device.GetRenderTarget(res.renderTargetDesc);
        return target;
    }

    BufferPtr RenderGraphResources::GetBuffer(string_view name) {
        return GetBuffer(GetID(name));
    }

    BufferPtr RenderGraphResources::GetBuffer(RenderGraphResourceID id) {
        if (id >= resources.size()) return nullptr;
        auto &res = resources[id];
        Assert(res.type == RenderGraphResource::Type::Buffer, "resource is not a buffer");
        auto &buf = buffers[res.id];
        if (!buf) buf = device.GetFramePooledBuffer(res.bufferDesc.type, res.bufferDesc.size);
        return buf;
    }

    vector<RenderGraph::RenderTargetInfo> RenderGraph::AllRenderTargets() {
        vector<RenderTargetInfo> output;
        for (const auto &[scope, names] : resources.nameScopes) {
            for (const auto &[name, id] : names) {
                const auto &res = resources.resources[id];
                if (res.type == RenderGraphResource::Type::RenderTarget) {
                    output.emplace_back(
                        RenderTargetInfo{scope.empty() ? name : scope + "." + name, res.renderTargetDesc});
                }
            }
        }
        return output;
    }

    const RenderGraphResource &RenderGraphResources::GetResource(string_view name) const {
        return GetResource(GetID(name, false));
    }

    const RenderGraphResource &RenderGraphResources::GetResource(RenderGraphResourceID id) const {
        if (id < resources.size()) return resources[id];
        static RenderGraphResource invalidResource = {};
        return invalidResource;
    }

    RenderGraphResourceID RenderGraphResources::GetID(string_view name, bool assertExists) const {
        RenderGraphResourceID result = npos;

        if (name.find('.') != string_view::npos) {
            // Any resource name with a dot is assumed to be fully qualified.
            auto lastDot = name.rfind('.');
            auto scopeName = name.substr(0, lastDot);
            auto resourceName = name.substr(lastDot + 1);

            for (auto &scope : nameScopes) {
                if (scope.name == scopeName) {
                    result = scope.GetID(resourceName);
                    break;
                }
            }
            Assert(!assertExists || result == npos, string("resource does not exist: ").append(name));
            return result;
        }

        for (auto scopeIt = scopeStack.rbegin(); scopeIt != scopeStack.rend(); scopeIt++) {
            auto id = nameScopes[*scopeIt].GetID(name);
            if (id != npos) return id;
        }
        Assert(!assertExists, string("resource does not exist: ").append(name));
        return npos;
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
        case RenderGraphResource::Type::Buffer:
            buffers[id].reset();
            break;
        default:
            Abort("resource type is undefined");
        }
    }

    void RenderGraphResources::Register(string_view name, RenderGraphResource &resource) {
        resource.id = (RenderGraphResourceID)resources.size();
        resources.push_back(resource);

        if (name.empty()) return;
        nameScopes[scopeStack.back()].SetID(name, resource.id);
    }

    RenderGraphResourceID RenderGraphResources::Scope::GetID(string_view name) const {
        auto it = resourceNames.find(name);
        if (it != resourceNames.end()) return it->second;
        return npos;
    }

    void RenderGraphResources::Scope::SetID(string_view name, RenderGraphResourceID id) {
        Assert(name.data()[name.size()] == '\0', "string_view is not null terminated");
        auto &nameID = resourceNames[name.data()];
        Assert(!nameID, "resource already registered");
        nameID = id;
    }

    void RenderGraph::BeginScope(string_view name) {
        auto &newScope = resources.nameScopes.emplace_back();
        const auto &topScope = resources.nameScopes[resources.scopeStack.back()];

        if (topScope.name.empty()) {
            newScope.name = name;
        } else {
            newScope.name = topScope.name + ".";
            newScope.name.append(name);
        }

        Assert(resources.scopeStack.size() < MAX_RESOURCE_SCOPE_DEPTH, "too many nested scopes");
        resources.scopeStack.push_back((uint8)(resources.nameScopes.size() - 1));
    }

    void RenderGraph::EndScope() {
        Assert(resources.scopeStack.size() > 1, "tried to end a scope that wasn't started");
        auto &scope = resources.nameScopes[resources.scopeStack.back()];
        scope.SetID("LastOutput", resources.LastOutputID());
        resources.scopeStack.pop_back();
    }

    void RenderGraph::SetTargetImageView(string_view name, ImageViewPtr view) {
        auto &res = resources.GetResource(name);
        Assert(res.renderTargetDesc.extent == view->Extent(), "image extent mismatch");

        auto resFormat = res.renderTargetDesc.format;
        auto viewFormat = view->Format();
        Assert(FormatComponentCount(resFormat) == FormatComponentCount(viewFormat), "image component count mismatch");
        Assert(FormatByteSize(resFormat) == FormatByteSize(viewFormat), "image component size mismatch");

        Assert(view->BaseArrayLayer() == 0, "view can't target a specific layer");
        Assert(res.renderTargetDesc.arrayLayers == view->ArrayLayers(), "image array mismatch");

        resources.ResizeBeforeExecute();
        resources.renderTargets[res.id] = make_shared<RenderTarget>(device, res.renderTargetDesc, view, ~0u);
    }

    void RenderGraph::Execute() {
        resources.ResizeBeforeExecute();
        resources.lastOutputID = RenderGraphResources::npos;

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

        std::stack<RenderPhase> phaseScopes;
        resources.scopeStack.clear();

        for (auto &pass : passes) {
            if (!pass.active) continue;

            Assert(pass.HasExecute(), "pass must have an Execute function");

            CommandContextPtr cmd;
            AddPassBarriers(cmd, pass); // creates cmd if necessary

            bool isRenderPass = false;
            RenderPassInfo renderPassInfo;

            for (uint32 i = 0; i < pass.attachments.size(); i++) {
                auto &attachment = pass.attachments[i];
                if (attachment.resourceID == ~0u) continue;
                isRenderPass = true;

                ImageViewPtr imageView;
                auto renderTarget = resources.GetRenderTarget(attachment.resourceID);
                if (attachment.arrayIndex != ~0u && renderTarget->Desc().arrayLayers > 1) {
                    imageView = renderTarget->LayerImageView(attachment.arrayIndex);
                } else {
                    imageView = renderTarget->ImageView();
                }

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

            for (int i = std::max(pass.scopes.size(), resources.scopeStack.size()) - 1; i >= 0; i--) {
                uint8 passScope = i < (int)pass.scopes.size() ? pass.scopes[i] : ~0u;
                uint8 resScope = i < (int)resources.scopeStack.size() ? resources.scopeStack[i] : ~0u;
                if (resScope != passScope) {
                    if (resScope != 255) phaseScopes.pop();
                    if (passScope != 255) {
                        string_view name = resources.nameScopes[passScope].name;
                        phaseScopes.emplace(name.empty() ? "RenderGraph" : name);
                        if (timer) phaseScopes.top().StartTimer(*timer);
                    }
                }
            }
            resources.scopeStack = pass.scopes;

            if (isRenderPass) {
                if (!cmd) cmd = device.GetCommandContext();
                {
                    RenderPhase phase(pass.name);
                    if (timer) phase.StartTimer(*cmd, *timer);
                    cmd->BeginRenderPass(renderPassInfo);
                    pass.Execute(resources, *cmd);
                    cmd->EndRenderPass();
                }
                device.Submit(cmd);
            } else if (pass.ExecutesWithDeviceContext()) {
                RenderPhase phase(pass.name);
                if (timer) phase.StartTimer(*timer);
                if (cmd) device.Submit(cmd);
                pass.Execute(resources, device);
            } else if (pass.ExecutesWithCommandContext()) {
                if (!cmd) cmd = device.GetCommandContext();
                {
                    RenderPhase phase(pass.name);
                    if (timer) phase.StartTimer(*cmd, *timer);
                    pass.Execute(resources, *cmd);
                }
                device.Submit(cmd);
            } else {
                Abort("invalid pass");
            }

            for (auto &dep : pass.dependencies) {
                resources.DecrementRef(dep.id);
            }

            pass.executeFunc = {}; // releases any captures
            UpdateLastOutput(pass);
        }
        passes.clear();
    }

    void RenderGraph::AddPassBarriers(CommandContextPtr &cmd, RenderGraphPass &pass) {
        for (auto &dep : pass.dependencies) {
            if (dep.access.layout == vk::ImageLayout::eUndefined) continue;

            auto &res = resources.resources[dep.id];
            Assert(res.type == RenderGraphResource::Type::RenderTarget, "resource type must be RenderTarget");

            auto image = resources.GetRenderTarget(dep.id)->ImageView()->Image();

            if (!cmd) cmd = device.GetCommandContext();
            cmd->ImageBarrier(image,
                image->LastLayout(),
                dep.access.layout,
                // TODO: infer these:
                vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                dep.access.stages,
                dep.access.access);
        }

        for (auto id : pass.outputs) {
            // assume pass doesn't depend on one of its outputs (e.g. blending), TODO: implement
            auto &res = resources.resources[id];

            if (res.type == RenderGraphResource::Type::RenderTarget) {
                auto view = resources.GetRenderTarget(id)->ImageView();
                auto image = view->Image();
                if (view->IsSwapchain()) continue; // barrier handled by RenderPass implicitly

                if (!cmd) cmd = device.GetCommandContext();

                if (res.renderTargetDesc.usage & vk::ImageUsageFlagBits::eColorAttachment) {
                    if (image->LastLayout() == vk::ImageLayout::eColorAttachmentOptimal) continue;
                    cmd->ImageBarrier(image,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eColorAttachmentOptimal,
                        vk::PipelineStageFlagBits::eBottomOfPipe,
                        {},
                        vk::PipelineStageFlagBits::eColorAttachmentOutput,
                        vk::AccessFlagBits::eColorAttachmentWrite);
                } else if (res.renderTargetDesc.usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) {
                    if (image->LastLayout() == vk::ImageLayout::eDepthStencilAttachmentOptimal) continue;
                    cmd->ImageBarrier(image,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eDepthStencilAttachmentOptimal,
                        vk::PipelineStageFlagBits::eBottomOfPipe,
                        {},
                        vk::PipelineStageFlagBits::eEarlyFragmentTests,
                        vk::AccessFlagBits::eDepthStencilAttachmentWrite);
                }
            }
        }
    }
} // namespace sp::vulkan
