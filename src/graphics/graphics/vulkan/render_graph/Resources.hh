#pragma once

#include "core/Hashing.hh"
#include "core/InlineVector.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/RenderTarget.hh"

#include <robin_hood.h>

namespace sp::vulkan::render_graph {
    typedef uint32 ResourceID;

    const uint32 MAX_RESOURCE_SCOPES = sizeof(uint8);
    const uint32 MAX_RESOURCE_SCOPE_DEPTH = 4;

    const ResourceID InvalidResource = ~0u;

    struct Resource {
        enum class Type {
            Undefined,
            RenderTarget,
            Buffer,
        };

        Resource() {}
        Resource(RenderTargetDesc desc) : type(Type::RenderTarget), renderTargetDesc(desc) {}
        Resource(BufferType bufType, size_t size) : type(Type::Buffer), bufferDesc({size, bufType}) {}

        explicit operator bool() const {
            return type != Type::Undefined;
        }

        RenderTargetDesc DeriveRenderTarget() const {
            Assert(type == Type::RenderTarget, "resource is not a render target");
            auto desc = renderTargetDesc;
            desc.usage = {};
            return desc;
        }

        vk::Format RenderTargetFormat() const {
            return renderTargetDesc.format;
        }

        ResourceID id = InvalidResource;
        Type type = Type::Undefined;

    private:
        union {
            RenderTargetDesc renderTargetDesc;
            BufferDesc bufferDesc;
        };

        friend class PassBuilder;
        friend class Resources;
        friend class RenderGraph;
    };

    class Resources {
    public:
        Resources(DeviceContext &device) : device(device) {
            Reset();
        }

        void Reset();

        RenderTargetPtr GetRenderTarget(ResourceID id);
        RenderTargetPtr GetRenderTarget(string_view name);

        BufferPtr GetBuffer(ResourceID id);
        BufferPtr GetBuffer(string_view name);

        const Resource &GetResource(string_view name) const;
        const Resource &GetResource(ResourceID id) const;
        ResourceID GetID(string_view name, bool assertExists = true) const;

        ResourceID LastOutputID() const {
            return lastOutputID;
        }
        const Resource &LastOutput() const {
            return GetResource(lastOutputID);
        }

        static const ResourceID npos = InvalidResource;

    private:
        friend class RenderGraph;
        friend class PassBuilder;

        void ResizeBeforeExecute();
        uint32 RefCount(ResourceID id);
        void IncrementRef(ResourceID id);
        void DecrementRef(ResourceID id);

        void Register(string_view name, Resource &resource);

        void BeginScope(string_view name);
        void EndScope();

        Resource &GetResourceRef(ResourceID id) {
            Assertf(id < resources.size(), "resource ID %u is invalid", id);
            return resources[id];
        }

        DeviceContext &device;

        struct Scope {
            string name;
            robin_hood::unordered_flat_map<string, ResourceID, StringHash, StringEqual> resourceNames;

            ResourceID GetID(string_view name) const;
            void SetID(string_view name, ResourceID id);
        };
        vector<Scope> nameScopes;
        InlineVector<uint8, MAX_RESOURCE_SCOPE_DEPTH> scopeStack; // refers to indexes in nameScopes

        vector<Resource> resources;

        // Built during execution
        vector<int32> refCounts;
        vector<RenderTargetPtr> renderTargets;
        vector<BufferPtr> buffers;

        ResourceID lastOutputID = npos;
    };
} // namespace sp::vulkan::render_graph
