/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "RenderGraph.hh"

#include "common/Logging.hh"
#include "graphics/vulkan/core/CommandContext.hh"
#include "graphics/vulkan/core/DeviceContext.hh"
#include "graphics/vulkan/core/PerfTimer.hh"
#include "graphics/vulkan/core/VkTracing.hh"

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
                if (pass.active) continue; // handled in a previous loop

                pass.active = pass.required;
                if (!pass.active) {
                    for (auto &access : pass.accesses) {
                        if (access.IsWrite() && resources.RefCount(access.id) > 0) {
                            pass.active = true;
                            break;
                        }
                    }
                }
                if (!pass.active) continue;

                for (auto &access : pass.accesses) {
                    resources.IncrementRef(access.id);
                    resources.AddUsageFromAccess(access.id, access.access);
                }
                for (auto &access : pass.futureReads) {
                    Assertf(access.framesFromNow < RESOURCE_FRAME_COUNT,
                        "Reading previous frame too far in the past: %d > %d",
                        access.framesFromNow,
                        RESOURCE_FRAME_COUNT - 1);
                    resources.IncrementRef(access.id);
                    resources.AddUsageFromAccess(access.id, access.access);
                    auto accessFrame = (resources.frameIndex + access.framesFromNow) % RESOURCE_FRAME_COUNT;
                    futureDependencies[accessFrame].push_back(access.id);
                    futureDependencyAdded = true;
                }
            }
        } while (futureDependencyAdded);

        auto timer = device.GetPerfTimer();

#ifdef TRACY_ENABLE_GRAPHICS
        std::stack<tracy::ScopedZone> traceScopes;
#endif
        std::stack<RenderPhase> phaseScopes;

        auto &frameScopeStack = resources.scopeStack;
        frameScopeStack.clear();

        vector<CommandContextPtr> pendingCmds;
        CommandContextPtr cmd;

        auto submitPendingCmds = [&](bool lastSubmit) {
            if (cmd) {
                pendingCmds.push_back(cmd);
                cmd.reset();
            }

            if (!pendingCmds.empty()) {
                device.Submit({(uint32)pendingCmds.size(), pendingCmds.data()}, {}, {}, {}, {}, lastSubmit);
                pendingCmds.clear();
            }
        };

        for (auto &pass : passes) {
            if (!pass.active) continue;
            Assert(pass.HasExecute(), "pass must have an Execute function");

            AddPreBarriers(cmd, pass); // creates cmd if necessary
            if (pass.flushCommands) submitPendingCmds(false);

            RenderPassInfo renderPassInfo;

            for (uint32 i = 0; i < pass.attachments.size(); i++) {
                auto &attachment = pass.attachments[i];
                if (attachment.resourceID == InvalidResource) continue;
                pass.isRenderPass = true;

                auto imageView = resources.GetImageView(attachment.resourceID);
                if (attachment.arrayIndex != ~0u && imageView->ArrayLayers() > 1) {
                    imageView = resources.GetImageLayerView(attachment.resourceID, attachment.arrayIndex);
                } else if (imageView->MipLevels() > 1) {
                    imageView = resources.GetImageMipView(attachment.resourceID, 0);
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
#ifdef TRACY_ENABLE_GRAPHICS
                        if (!traceScopes.empty()) traceScopes.pop();
#endif
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

#ifdef TRACY_ENABLE_GRAPHICS
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
#endif
                    }
                }
            }
            frameScopeStack = pass.scopes;

#ifdef TRACY_ENABLE_GRAPHICS
            tracy::ScopedZone traceZone(__LINE__,
                __FILE__,
                strlen(__FILE__),
                __FUNCTION__,
                strlen(__FUNCTION__),
                pass.name.data(),
                pass.name.size(),
                true);
#endif

            if (pass.isRenderPass) {
                if (!cmd) cmd = device.GetFrameCommandContext(resources);
                GPUZoneTransient(&device, cmd, traceVkZone, pass.name.data(), pass.name.size());
                RenderPhase phase(pass.name);
                phase.StartTimer(*cmd);
                cmd->BeginRenderPass(renderPassInfo);
                pass.Execute(resources, *cmd);
                cmd->EndRenderPass();
            } else if (pass.ExecutesWithDeviceContext()) {
                RenderPhase phase(pass.name);
                if (timer) phase.StartTimer(*timer);
                pass.Execute(resources, device);
            } else if (pass.ExecutesWithCommandContext()) {
                if (!cmd) cmd = device.GetFrameCommandContext(resources);
                GPUZoneTransient(&device, cmd, traceVkZone, pass.name.data(), pass.name.size());
                RenderPhase phase(pass.name);
                phase.StartTimer(*cmd);
                pass.Execute(resources, *cmd);
            } else {
                Abortf("invalid render graph pass: %s", pass.name);
            }

            if (cmd) pendingCmds.push_back(std::move(cmd));
            cmd.reset();

            for (auto &access : pass.accesses) {
                resources.DecrementRef(access.id);
            }

            pass.executeFunc = {}; // releases any captures
            UpdateLastOutput(pass);
        }
        for (auto &id : futureDependencies[resources.frameIndex]) {
            resources.DecrementRef(id);
        }
        futureDependencies[resources.frameIndex].clear();

        submitPendingCmds(true);
        AdvanceFrame();
    }

    void RenderGraph::AddPreBarriers(CommandContextPtr &cmd, Pass &pass) {
        ZoneScoped;
        for (auto &access : pass.accesses) {
            auto nextAccess = access.access;
            if (nextAccess == Access::None || nextAccess >= Access::AccessTypesCount) continue;
            auto &next = GetAccessInfo(nextAccess);

            auto &res = resources.resources[access.id];
            Assertf(res.type != Resource::Type::Undefined, "undefined resource access %d", access.id);

            if (res.type == Resource::Type::Image) {
                auto view = resources.GetImageView(access.id);
                if (!view || view->IsSwapchain()) continue; // barrier handled by RenderPass implicitly

                auto &image = view->Image();
                auto lastAccess = image->LastAccess();
                if (next.imageLayout == vk::ImageLayout::eUndefined && lastAccess == Access::None) continue;

                auto last = GetAccessInfo(lastAccess);
                if (last.stageMask == vk::PipelineStageFlags(0)) last.stageMask = vk::PipelineStageFlagBits::eTopOfPipe;
                if (nextAccess == Access::ColorAttachmentWrite) last.imageLayout = vk::ImageLayout::eUndefined;

                if (!cmd) cmd = device.GetFrameCommandContext(resources);

                cmd->ImageBarrier(image,
                    last.imageLayout,
                    next.imageLayout,
                    last.stageMask,
                    last.accessMask,
                    next.stageMask,
                    next.accessMask);

                image->SetAccess(Access::None, nextAccess);
            } else if (res.type == Resource::Type::Buffer) {
                auto buffer = resources.GetBuffer(access.id);
                auto lastAccess = buffer->LastAccess();
                buffer->SetAccess(Access::None, nextAccess);

                if (nextAccess == Access::HostWrite) continue;
                if (lastAccess == Access::None) continue;

                if (!cmd) cmd = device.GetFrameCommandContext(resources);

                auto last = GetAccessInfo(lastAccess);
                vk::MemoryBarrier barrier;
                barrier.srcAccessMask = last.accessMask;
                barrier.dstAccessMask = next.accessMask;
                cmd->Raw().pipelineBarrier(last.stageMask, next.stageMask, {}, {barrier}, {}, {});
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
        Assert(res.imageDesc.extent == view->Extent(), "image extent mismatch");

        auto resFormat = res.imageDesc.format;
        auto viewFormat = view->Format();
        Assert(FormatComponentCount(resFormat) == FormatComponentCount(viewFormat), "image component count mismatch");
        Assert(FormatByteSize(resFormat) == FormatByteSize(viewFormat), "image component size mismatch");

        Assert(view->BaseArrayLayer() == 0, "view can't target a specific layer");
        Assert(res.imageDesc.arrayLayers == view->ArrayLayers(), "image array mismatch");

        resources.ResizeIfNeeded();
        resources.images[res.id] = make_shared<PooledImage>(device, res.imageDesc, view);
    }

    vector<RenderGraph::PooledImageInfo> RenderGraph::AllImages() {
        vector<PooledImageInfo> output;
        for (const auto &[scope, frame] : resources.nameScopes) {
            for (const auto &[name, id] : frame[resources.frameIndex].resourceNames) {
                const auto &res = resources.resources[id];
                if (res.type == Resource::Type::Image) {
                    output.emplace_back(
                        PooledImageInfo{ResourceName{scope.empty() ? name : scope + "." + name}, res.imageDesc});
                }
            }
        }
        return output;
    }
} // namespace sp::vulkan::render_graph
