#include "RenderGraph.hh"

#include "core/Logging.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"
#include "graphics/vulkan/core/Tracing.hh"

namespace sp::vulkan::render_graph {
    RenderGraph::RenderGraph(DeviceContext &device) : device(device), resources(device) {}

    void RenderGraph::Execute() {
        ZoneScoped;
        resources.ResizeIfNeeded();
        resources.lastOutputID = InvalidResource;

        // passes are already sorted by dependency order
        bool futureDependencyAdded;
        do {
            futureDependencyAdded = false;
            for (auto it = passes.rbegin(); it != passes.rend(); it++) {
                auto &pass = *it;
                if (pass.active) continue;
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
                for (auto &dep : pass.futureDependencies) {
                    resources.IncrementRef(dep.id);
                    auto depFrame = (resources.frameIndex + dep.framesFromNow) % RESOURCE_FRAME_COUNT;
                    futureDependencies[depFrame].push_back(dep.id);
                    futureDependencyAdded = true;
                }
            }
        } while (futureDependencyAdded);

        for (auto id : futureDependencies[resources.frameIndex]) {
            resources.DecrementRef(id);
        }
        futureDependencies[resources.frameIndex].clear();

        auto timer = device.GetPerfTimer();

        std::stack<tracy::ScopedZone> traceScopes;
        std::stack<RenderPhase> phaseScopes;

        auto &frameScopeStack = resources.scopeStack;
        frameScopeStack.clear();

        vector<CommandContextPtr> pendingCmds;
        CommandContextPtr cmd;

        auto submitPendingCmds = [&]() {
            if (cmd) {
                pendingCmds.push_back(cmd);
                cmd.reset();
            }

            if (!pendingCmds.empty()) {
                device.Submit({(uint32)pendingCmds.size(), pendingCmds.data()});
                pendingCmds.clear();
            }
        };

        for (auto &pass : passes) {
            if (!pass.active) continue;

            Assert(pass.HasExecute(), "pass must have an Execute function");

            for (auto out : pass.outputs) {
                resources.IncrementRef(out);
            }

            AddPassBarriers(cmd, pass); // creates cmd if necessary

            RenderPassInfo renderPassInfo;

            for (uint32 i = 0; i < pass.attachments.size(); i++) {
                auto &attachment = pass.attachments[i];
                if (attachment.resourceID == InvalidResource) continue;
                pass.isRenderPass = true;

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

            for (int i = std::max(pass.scopes.size(), frameScopeStack.size()) - 1; i >= 0; i--) {
                uint8 passScope = i < (int)pass.scopes.size() ? pass.scopes[i] : 255;
                uint8 resScope = i < (int)frameScopeStack.size() ? frameScopeStack[i] : 255;
                if (resScope != passScope) {
                    if (resScope != 255) {
                        if (!traceScopes.empty()) traceScopes.pop();
                        phaseScopes.pop();
                    }
                    if (passScope != 255) {
                        string_view name = resources.nameScopes[passScope].name;
                        if (!name.empty()) {
                            auto dot = name.find(".");
                            if (dot != name.npos) name = name.substr(dot + 1);
                        }
                        phaseScopes.emplace(name.empty() ? "RenderGraph" : name);
                        if (timer) phaseScopes.top().StartTimer(*timer);

                        if (!name.empty()) {
                            traceScopes.emplace(__LINE__,
                                __FILE__,
                                strlen(__FILE__),
                                __FUNCTION__,
                                strlen(__FUNCTION__),
                                name.data(),
                                name.size(),
                                true);
                        }
                    }
                }
            }
            frameScopeStack = pass.scopes;

            tracy::ScopedZone traceZone(__LINE__,
                __FILE__,
                strlen(__FILE__),
                __FUNCTION__,
                strlen(__FUNCTION__),
                pass.name.data(),
                pass.name.size(),
                true);

            if (pass.isRenderPass) {
                if (!cmd) cmd = device.GetFrameCommandContext();
                GPUZoneTransient(&device, cmd, traceVkZone, pass.name.data(), pass.name.size());
                RenderPhase phase(pass.name);
                phase.StartTimer(*cmd);
                cmd->BeginRenderPass(renderPassInfo);
                pass.Execute(resources, *cmd);
                cmd->EndRenderPass();
            } else if (pass.ExecutesWithDeviceContext()) {
                submitPendingCmds();
                RenderPhase phase(pass.name);
                if (timer) phase.StartTimer(*timer);
                pass.Execute(resources, device);
            } else if (pass.ExecutesWithCommandContext()) {
                if (!cmd) cmd = device.GetFrameCommandContext();
                GPUZoneTransient(&device, cmd, traceVkZone, pass.name.data(), pass.name.size());
                RenderPhase phase(pass.name);
                phase.StartTimer(*cmd);
                pass.Execute(resources, *cmd);
            } else {
                Abort("invalid pass");
            }

            if (cmd) pendingCmds.push_back(std::move(cmd));
            cmd.reset();

            for (auto &dep : pass.dependencies) {
                resources.DecrementRef(dep.id);
            }
            for (auto out : pass.outputs) {
                resources.DecrementRef(out);
            }

            pass.executeFunc = {}; // releases any captures
            UpdateLastOutput(pass);

            submitPendingCmds();
        }

        submitPendingCmds();
        AdvanceFrame();
    }

    void RenderGraph::AddPassBarriers(CommandContextPtr &cmd, Pass &pass) {
        for (auto &dep : pass.dependencies) {
            if (dep.access.layout == vk::ImageLayout::eUndefined) continue;

            auto &res = resources.resources[dep.id];
            Assert(res.type == Resource::Type::RenderTarget, "resource type must be RenderTarget");

            auto image = resources.GetRenderTarget(dep.id)->ImageView()->Image();

            if (!cmd) cmd = device.GetFrameCommandContext();
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

            if (res.type == Resource::Type::RenderTarget) {
                auto view = resources.GetRenderTarget(id)->ImageView();
                auto image = view->Image();
                if (view->IsSwapchain()) continue; // barrier handled by RenderPass implicitly

                if (!cmd) cmd = device.GetFrameCommandContext();

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

    void RenderGraph::AdvanceFrame() {
        passes.clear();
        resources.AdvanceFrame();
    }

    void RenderGraph::BeginScope(string_view name) {
        resources.BeginScope(name);
    }

    void RenderGraph::EndScope() {
        resources.EndScope();
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

        resources.ResizeIfNeeded();
        resources.renderTargets[res.id] = make_shared<RenderTarget>(device, res.renderTargetDesc, view, ~0u);
    }

    vector<RenderGraph::RenderTargetInfo> RenderGraph::AllRenderTargets() {
        vector<RenderTargetInfo> output;
        for (const auto &[scope, frame] : resources.nameScopes) {
            for (const auto &[name, id] : frame[resources.frameIndex].resourceNames) {
                const auto &res = resources.resources[id];
                if (res.type == Resource::Type::RenderTarget) {
                    output.emplace_back(
                        RenderTargetInfo{scope.empty() ? name : scope + "." + name, res.renderTargetDesc});
                }
            }
        }
        return output;
    }
} // namespace sp::vulkan::render_graph
