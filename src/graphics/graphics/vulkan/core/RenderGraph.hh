#pragma once

#include "core/StackVector.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/RenderPass.hh"
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
        uint32 resourceID = ~0u;
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
        uint32 RefCount(uint32 id);
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
        friend class RenderGraphPassBuilder;
        string_view name;
        StackVector<RenderGraphResourceDependency, 16> dependencies;
        StackVector<uint32, 16> outputs;
        std::array<AttachmentInfo, MAX_COLOR_ATTACHMENTS + 1> attachments;
        bool active = false, required = false;
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

        const RenderGraphResource &ShaderRead(string_view name) {
            return ShaderRead(resources.GetID(name));
        }
        const RenderGraphResource &ShaderRead(uint32 id) {
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
        const RenderGraphResource &TransferRead(uint32 id) {
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

        void RequireResource(string_view name) {
            RequireResource(resources.GetResourceByName(name).id);
        }

        void RequireResource(uint32 id) {
            resources.IncrementRef(id);
        }

        void Execute();

    private:
        void AddPassBarriers(CommandContext &cmd, RenderGraphPassBase *pass);

        DeviceContext &device;
        RenderGraphResources resources;
        vector<shared_ptr<RenderGraphPassBase>> passes;
    };
} // namespace sp::vulkan
