#pragma once

#include "core/StackVector.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/RenderTarget.hh"

#include <robin_hood.h>

namespace sp::vulkan {
    struct RenderGraphResource {
        enum class Type {
            Undefined,
            RenderTarget,
        };

        RenderGraphResource() {}
        RenderGraphResource(RenderTargetDesc desc) : type(Type::RenderTarget), renderTargetDesc(desc) {}

        uint32 id = ~0u;
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

    class RenderGraphResources {
    public:
        RenderGraphResources(DeviceContext &device) : device(device) {}

        RenderTargetPtr GetRenderTarget(uint32 id);
        RenderTargetPtr GetRenderTarget(string_view name);

        const RenderGraphResource &GetResourceByName(string_view name) const;

        uint32 GetID(string_view name) const {
            Assert(name.data()[name.size()] == '\0', "string_view is not null terminated");
            auto it = names.find(name.data());
            if (it == names.end()) return ~0u;
            return it->second;
        }

    private:
        friend class RenderGraph;
        friend class RenderGraphPassBuilder;

        void ResizeBeforeExecute();
        void IncrementRef(uint32 id);
        void DecrementRef(uint32 id);

        void Register(string_view name, RenderGraphResource &resource) {
            resource.id = (uint32)resources.size();
            resources.push_back(resource);

            Assert(name.data()[name.size()] == '\0', "string_view is not null terminated");
            auto &nameID = names[name.data()];
            Assert(!nameID, "resource already registered");
            nameID = resource.id;
        }

        RenderGraphResource &GetResourceRef(uint32 id) {
            Assert(id < resources.size(), "resource ID " + std::to_string(id) + " is invalid");
            return resources[id];
        }

        DeviceContext &device;
        robin_hood::unordered_flat_map<string, uint32> names;
        vector<RenderGraphResource> resources;

        // Built during execution
        vector<uint32> refCounts;
        vector<RenderTargetPtr> renderTargets;
    };

    struct RenderGraphResourceDependency {
        RenderGraphResourceAccess access;
        uint32 id;
    };

    class RenderGraphPassBase {
    public:
        RenderGraphPassBase(string_view name) : name(name) {}
        virtual ~RenderGraphPassBase() {}
        virtual void Execute(RenderGraphResources &resources, CommandContext &cmd) = 0;

        void AddDependency(const RenderGraphResourceAccess &access, const RenderGraphResource &res) {
            dependencies.push({access, res.id});
        }

        void AddOutput(const RenderGraphResource &res) {
            outputs.push(res.id);
        }

    private:
        friend class RenderGraph;
        string_view name;
        StackVector<RenderGraphResourceDependency, 16> dependencies;
        StackVector<uint32, 16> outputs;
    };

    template<typename ExecuteFunc>
    class RenderGraphPass : public RenderGraphPassBase {
    public:
        RenderGraphPass(string_view name, ExecuteFunc &executeFunc)
            : RenderGraphPassBase(name), executeFunc(executeFunc) {}

        void Execute(RenderGraphResources &resources, CommandContext &cmd) override {
            executeFunc(resources, cmd);
        }

    private:
        ExecuteFunc executeFunc;
    };

    class RenderGraphPassBuilder {
    public:
        RenderGraphPassBuilder(RenderGraphResources &resources, RenderGraphPassBase &pass)
            : resources(resources), pass(pass) {}

        void ShaderRead(string_view name) {
            ShaderRead(resources.GetID(name));
        }
        void ShaderRead(uint32 id) {
            auto &resource = resources.GetResourceRef(id);
            resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eSampled;
            RenderGraphResourceAccess access = {
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::AccessFlagBits::eShaderRead,
                vk::ImageLayout::eShaderReadOnlyOptimal,
            };
            pass.AddDependency(access, resource);
        }

        void TransferRead(string_view name) {
            TransferRead(resources.GetID(name));
        }
        void TransferRead(uint32 id) {
            auto &resource = resources.GetResourceRef(id);
            resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eTransferSrc;
            RenderGraphResourceAccess access = {
                vk::PipelineStageFlagBits::eTransfer,
                vk::AccessFlagBits::eTransferRead,
                vk::ImageLayout::eTransferSrcOptimal,
            };
            pass.AddDependency(access, resource);
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

    private:
        RenderGraphResources &resources;
        RenderGraphPassBase &pass;
    };

    class RenderGraph {
    public:
        RenderGraph(DeviceContext &device) : device(device), resources(device) {}

        template<typename SetupFunc, typename ExecuteFunc>
        void AddPass(string_view name, SetupFunc setupFunc, ExecuteFunc executeFunc) {
            static_assert(sizeof(ExecuteFunc) < 1024, "execute callback must capture less than 1kB");

            auto pass = make_shared<RenderGraphPass<ExecuteFunc>>(name, executeFunc);
            RenderGraphPassBuilder builder(resources, *pass);
            setupFunc(builder);
            passes.push_back(pass);
        }

        void SetTargetImageView(string_view name, ImageViewPtr view);

        void Execute();

    private:
        void AddPassBarriers(CommandContext &cmd, RenderGraphPassBase *pass);

        DeviceContext &device;
        RenderGraphResources resources;
        vector<shared_ptr<RenderGraphPassBase>> passes;
    };
} // namespace sp::vulkan
