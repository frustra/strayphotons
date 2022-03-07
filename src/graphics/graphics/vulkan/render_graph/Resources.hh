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
    const uint32 RESOURCE_FRAME_COUNT = 2;

    const ResourceID InvalidResource = ~0u;

    struct Resource {
        enum class Type {
            Undefined,
            RenderTarget,
            Buffer,
        };

        Resource() {}
        Resource(RenderTargetDesc desc) : type(Type::RenderTarget), renderTargetDesc(desc) {}
        Resource(BufferDesc desc) : type(Type::Buffer), bufferDesc(desc) {}

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
            nameScopes.emplace_back();
        }

        RenderTargetPtr GetRenderTarget(ResourceID id);
        RenderTargetPtr GetRenderTarget(string_view name);

        BufferPtr GetBuffer(ResourceID id);
        BufferPtr GetBuffer(string_view name);

        const Resource &GetResource(string_view name) const;
        const Resource &GetResource(ResourceID id) const;
        ResourceID GetID(string_view name, bool assertExists = true, int framesAgo = 0) const;

        ResourceID LastOutputID() const {
            return lastOutputID;
        }
        const Resource &LastOutput() const {
            return GetResource(lastOutputID);
        }

        /**
         * Keeps a resource alive until the next frame's graph has been built.
         * If no pass depends on the resource in the next frame, it will be released before execution.
         */
        void ExportToNextFrame(ResourceID id);
        void ExportToNextFrame(string_view name);

    private:
        friend class RenderGraph;
        friend class PassBuilder;

        void ResizeIfNeeded();
        void DecrefExportedResources();

        uint32 RefCount(ResourceID id);
        void IncrementRef(ResourceID id);
        void DecrementRef(ResourceID id);

        void Register(string_view name, Resource &resource);

        void BeginScope(string_view name);
        void EndScope();

        void AdvanceFrame();
        void Reset();

        Resource &GetResourceRef(ResourceID id) {
            Assertf(id < resources.size(), "resource ID %u is invalid", id);
            return resources[id];
        }

        DeviceContext &device;
        uint32 frameIndex = 0;

        struct Scope {
            string name;

            struct PerFrame {
                // robin_hood::unordered_flat_map<string, ResourceID, StringHash, StringEqual> resourceNames;
                std::unordered_map<string, ResourceID, StringHash, StringEqual> resourceNames;
            };
            std::array<PerFrame, RESOURCE_FRAME_COUNT> frames;

            ResourceID GetID(string_view name, uint32 frameIndex) const;
            void SetID(string_view name, ResourceID id, uint32 frameIndex);
        };

        vector<Scope> nameScopes;
        InlineVector<uint8, MAX_RESOURCE_SCOPE_DEPTH> scopeStack; // refers to indexes in nameScopes

        vector<Resource> resources;
        vector<ResourceID> freeIDs, exportedIDs, lastExportedIDs;
        size_t lastResourceCount = 0, consecutiveGrowthFrames = 0;

        vector<int32> refCounts;
        vector<RenderTargetPtr> renderTargets;
        vector<BufferPtr> buffers;

        ResourceID lastOutputID = InvalidResource;
    };
} // namespace sp::vulkan::render_graph
