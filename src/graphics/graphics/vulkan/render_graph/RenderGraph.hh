#pragma once

#include "graphics/vulkan/render_graph/Pass.hh"
#include "graphics/vulkan/render_graph/PassBuilder.hh"
#include "graphics/vulkan/render_graph/Resources.hh"

namespace sp::vulkan {
    namespace rg = render_graph;
} // namespace sp::vulkan

namespace sp::vulkan::render_graph {
    const uint32 RESOURCE_HISTORY_FRAMES = 1;

    class RenderGraph {
    public:
        RenderGraph(DeviceContext &device) : device(device), resourceFrames({device, device}) {
            frameIndex = 0;
            resources = &resourceFrames[frameIndex];
        }

        class InitialPassState {
        public:
            InitialPassState(RenderGraph &graph, string_view name) : graph(graph), name(name) {}

            template<typename SetupFunc>
            InitialPassState &Build(SetupFunc setupFunc) {
                Assert(passIndex == ~0u, "multiple Build calls for the same pass");
                Pass pass(name);
                pass.scopes = graph.resources->scopeStack;

                PassBuilder builder(*graph.resources, pass);
                setupFunc(builder);
                passIndex = graph.passes.size();
                graph.passes.push_back(pass);
                graph.UpdateLastOutput(pass);
                return *this;
            }

            ResourceID Execute(std::function<void(Resources &, CommandContext &)> executeFunc) {
                Assert(passIndex != ~0u, "Build must be called before Execute");
                Assert(executeFunc, "Execute function must be defined");
                auto &pass = graph.passes[passIndex];
                Assert(!pass.HasExecute(), "multiple Execute functions for the same pass");
                pass.executeFunc = executeFunc;
                return graph.LastOutputID();
            }

            ResourceID Execute(std::function<void(Resources &, DeviceContext &)> executeFunc) {
                Assert(passIndex != ~0u, "Build must be called before Execute");
                Assert(executeFunc, "Execute function must be defined");
                auto &pass = graph.passes[passIndex];
                Assert(!pass.HasExecute(), "multiple Execute functions for the same pass");
                pass.executeFunc = executeFunc;
                return graph.LastOutputID();
            }

        private:
            RenderGraph &graph;
            string_view name;
            uint32 passIndex = ~0u;
        };

        InitialPassState AddPass(string_view name) {
            return {*this, name};
        }

        class GraphScope {
            RenderGraph &graph;

        public:
            GraphScope(RenderGraph &graph, string_view name) : graph(graph) {
                graph.BeginScope(name);
            }
            ~GraphScope() {
                graph.EndScope();
            }
        };

        GraphScope Scope(string_view name) {
            return {*this, name};
        }

        void BeginScope(string_view name);
        void EndScope();

        void SetTargetImageView(string_view name, ImageViewPtr view);

        void RequireResource(string_view name) {
            RequireResource(resources->GetID(name));
        }

        void RequireResource(ResourceID id) {
            resources->IncrementRef(id);
        }

        void Execute();

        struct RenderTargetInfo {
            string name;
            RenderTargetDesc desc;
        };
        vector<RenderTargetInfo> AllRenderTargets();

        ResourceID LastOutputID() const {
            return resources->lastOutputID;
        }
        Resource LastOutput() const {
            return resources->LastOutput();
        }

        bool HasResource(string_view name) const {
            return resources->GetID(name, false) != InvalidResource;
        }

        DeviceContext &Device() {
            return device;
        }

    private:
        friend class InitialPassState;
        void AddPassBarriers(CommandContextPtr &cmd, Pass &pass);
        void AdvanceFrame();

        void UpdateLastOutput(const Pass &pass) {
            if (pass.attachments.size() > pass.primaryAttachmentIndex) {
                resources->lastOutputID = pass.attachments[pass.primaryAttachmentIndex].resourceID;
            }
        }

        DeviceContext &device;

        vector<Pass> passes;

        // Points to an entry in resourceFrames based on the current frame index
        Resources *resources;
        std::array<Resources, RESOURCE_HISTORY_FRAMES + 1> resourceFrames;
        size_t frameIndex;
    };
} // namespace sp::vulkan::render_graph
