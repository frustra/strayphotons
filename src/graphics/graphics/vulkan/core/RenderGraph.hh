#pragma once

#include "core/StackVector.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/RenderPass.hh"
#include "graphics/vulkan/core/RenderTarget.hh"

#include <robin_hood.h>
#include <variant>

namespace sp::vulkan {
    struct RenderGraphResource {
        enum class Type {
            Undefined,
            RenderTarget,
        };

        RenderGraphResource() {}
        RenderGraphResource(RenderTargetDesc desc) : type(Type::RenderTarget), renderTargetDesc(desc) {}

        RenderGraphResourceID id = ~0u;
        Type type = Type::Undefined;
        union {
            RenderTargetDesc renderTargetDesc;
        };
    };

    struct RenderGraphResourceAccess {
        vk::PipelineStageFlags stages = {};
        vk::AccessFlags access = {};
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    };

    struct AttachmentInfo {
        AttachmentInfo() {}
        AttachmentInfo(LoadOp loadOp, StoreOp storeOp) : loadOp(loadOp), storeOp(storeOp) {}
        LoadOp loadOp = LoadOp::DontCare;
        StoreOp storeOp = StoreOp::DontCare;
        vk::ClearColorValue clearColor = {};
        vk::ClearDepthStencilValue clearDepthStencil = {1.0f, 0};

        void SetClearColor(glm::vec4 clear) {
            std::array<float, 4> clearValues = {clear.r, clear.g, clear.b, clear.a};
            clearColor = {clearValues};
        }

    private:
        friend class RenderGraph;
        friend class RenderGraphPassBuilder;
        RenderGraphResourceID resourceID = ~0u;
    };

    class RenderGraphResources {
    public:
        RenderGraphResources(DeviceContext &device) : device(device) {}

        RenderTargetPtr GetRenderTarget(RenderGraphResourceID id);
        RenderTargetPtr GetRenderTarget(string_view name);

        const RenderGraphResource &GetResourceByName(string_view name) const;
        const RenderGraphResource &GetResourceByID(RenderGraphResourceID id) const;

        RenderGraphResourceID GetID(string_view name) const {
            Assert(name.data()[name.size()] == '\0', "string_view is not null terminated");
            auto it = names.find(name.data());
            if (it == names.end()) return ~0u;
            return it->second;
        }

    private:
        friend class RenderGraph;
        friend class RenderGraphPassBuilder;

        void ResizeBeforeExecute();
        uint32 RefCount(RenderGraphResourceID id);
        void IncrementRef(RenderGraphResourceID id);
        void DecrementRef(RenderGraphResourceID id);

        void Register(string_view name, RenderGraphResource &resource) {
            resource.id = (RenderGraphResourceID)resources.size();
            resources.push_back(resource);

            if (name.empty()) return;
            Assert(name.data()[name.size()] == '\0', "string_view is not null terminated");
            auto &nameID = names[name.data()];
            Assert(!nameID, "resource already registered");
            nameID = resource.id;
        }

        RenderGraphResource &GetResourceRef(RenderGraphResourceID id) {
            Assert(id < resources.size(), "resource ID " + std::to_string(id) + " is invalid");
            return resources[id];
        }

        DeviceContext &device;
        robin_hood::unordered_flat_map<string, RenderGraphResourceID> names;
        vector<RenderGraphResource> resources;

        // Built during execution
        vector<uint32> refCounts;
        vector<RenderTargetPtr> renderTargets;
    };

    struct RenderGraphResourceDependency {
        RenderGraphResourceAccess access;
        RenderGraphResourceID id;
    };

    class RenderGraphPass {
    public:
        RenderGraphPass(string_view name) : name(name) {}

        void AddDependency(const RenderGraphResourceAccess &access, const RenderGraphResource &res) {
            dependencies.push({access, res.id});
        }

        void AddOutput(const RenderGraphResource &res) {
            outputs.push(res.id);
        }

        bool ExecutesWithCommandContext() const {
            return executeFunc.index() == 0;
        }
        bool ExecutesWithDeviceContext() const {
            return executeFunc.index() == 1;
        }
        void Execute(RenderGraphResources &resources, CommandContext &cmd) const {
            auto &f = std::get<0>(executeFunc);
            Assert(f, "execute function with CommandContext not defined");
            f(resources, cmd);
        }
        void Execute(RenderGraphResources &resources, DeviceContext &device) const {
            auto &f = std::get<1>(executeFunc);
            Assert(f, "execute function with DeviceContext not defined");
            f(resources, device);
        }

    private:
        friend class RenderGraph;
        friend class RenderGraphPassBuilder;
        string_view name;
        StackVector<RenderGraphResourceDependency, 16> dependencies;
        StackVector<RenderGraphResourceID, 16> outputs;
        std::array<AttachmentInfo, MAX_COLOR_ATTACHMENTS + 1> attachments;
        bool active = false, required = false;

        std::variant<std::function<void(RenderGraphResources &, CommandContext &)>,
                     std::function<void(RenderGraphResources &, DeviceContext &)>>
            executeFunc;
    };

    class RenderGraphPassBuilder {
    public:
        RenderGraphPassBuilder(RenderGraphResources &resources, RenderGraphPass &pass)
            : resources(resources), pass(pass) {}

        const RenderGraphResource &ShaderRead(string_view name) {
            return ShaderRead(resources.GetID(name));
        }
        const RenderGraphResource &ShaderRead(RenderGraphResourceID id) {
            auto &resource = resources.GetResourceRef(id);
            resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eSampled;
            RenderGraphResourceAccess access = {
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eShaderReadOnlyOptimal,
            };
            pass.AddDependency(access, resource);
            return resource;
        }

        const RenderGraphResource &TransferRead(string_view name) {
            return TransferRead(resources.GetID(name));
        }
        const RenderGraphResource &TransferRead(RenderGraphResourceID id) {
            auto &resource = resources.GetResourceRef(id);
            resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eTransferSrc;
            RenderGraphResourceAccess access = {
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferRead,
                vk::ImageLayout::eTransferSrcOptimal,
            };
            pass.AddDependency(access, resource);
            return resource;
        }

        RenderGraphResource GetResourceByName(string_view name) {
            return resources.GetResourceByName(name);
        }

        RenderGraphResource OutputRenderTarget(string_view name, const RenderTargetDesc &desc) {
            auto resource = RenderGraphResource(desc);
            resources.Register(name, resource);
            pass.AddOutput(resource);
            return resource;
        }

        RenderGraphResource OutputColorAttachment(uint32 index,
                                                  string_view name,
                                                  RenderTargetDesc desc,
                                                  const AttachmentInfo &info) {
            desc.usage |= vk::ImageUsageFlagBits::eColorAttachment;
            return OutputAttachment(index, name, desc, info);
        }

        RenderGraphResource OutputDepthAttachment(string_view name, RenderTargetDesc desc, const AttachmentInfo &info) {
            desc.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
            return OutputAttachment(MAX_COLOR_ATTACHMENTS, name, desc, info);
        }

        void RequirePass() {
            pass.required = true;
        }

    private:
        RenderGraphResource OutputAttachment(uint32 index,
                                             string_view name,
                                             const RenderTargetDesc &desc,
                                             const AttachmentInfo &info) {
            auto resource = OutputRenderTarget(name, desc);
            auto &attachment = pass.attachments[index];
            attachment = info;
            attachment.resourceID = resource.id;
            return resource;
        }

        RenderGraphResources &resources;
        RenderGraphPass &pass;
    };

    class RenderGraph {
    public:
        RenderGraph(DeviceContext &device) : device(device), resources(device) {}

        class InitialPassState {
        public:
            InitialPassState(RenderGraph &graph, string_view name) : graph(graph), name(name) {}

            template<typename SetupFunc>
            InitialPassState &Build(SetupFunc setupFunc) {
                Assert(passIndex == ~0u, "multiple build calls for the same pass");

                RenderGraphPass pass(name);
                RenderGraphPassBuilder builder(graph.resources, pass);
                setupFunc(builder);
                passIndex = graph.passes.size();
                graph.passes.push_back(pass);
                return *this;
            }

            InitialPassState &Execute(std::function<void(RenderGraphResources &, CommandContext &)> executeFunc) {
                Assert(passIndex != ~0u, "build must be called before execute");

                auto &pass = graph.passes[passIndex];
                Assert(!hasExecute, "multiple execution functions for the same pass");
                pass.executeFunc = executeFunc;
                hasExecute = true;
                return *this;
            }

            InitialPassState &Execute(std::function<void(RenderGraphResources &, DeviceContext &)> executeFunc) {
                Assert(passIndex != ~0u, "build must be called before execute");

                auto &pass = graph.passes[passIndex];
                Assert(!hasExecute, "multiple execution functions for the same pass");
                pass.executeFunc = executeFunc;
                hasExecute = true;
                return *this;
            }

        private:
            RenderGraph &graph;
            string_view name;
            uint32 passIndex = ~0u;
            bool hasExecute = false;
        };

        InitialPassState Pass(string_view name) {
            return {*this, name};
        }

        void SetTargetImageView(string_view name, ImageViewPtr view);

        void RequireResource(string_view name) {
            RequireResource(resources.GetResourceByName(name).id);
        }

        void RequireResource(RenderGraphResourceID id) {
            resources.IncrementRef(id);
        }

        void Execute();

    private:
        friend class InitialPassState;
        void AddPassBarriers(CommandContext &cmd, RenderGraphPass &pass);

        DeviceContext &device;
        RenderGraphResources resources;
        vector<RenderGraphPass> passes;
    };
} // namespace sp::vulkan
