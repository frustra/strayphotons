#pragma once

#include "core/Hashing.hh"
#include "core/InlineVector.hh"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/render_graph/PooledImage.hh"

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
            Image,
            Buffer,
        };

        Resource() {}
        Resource(ImageDesc desc) : type(Type::Image), imageDesc(desc) {}
        Resource(BufferDesc desc) : type(Type::Buffer), bufferDesc(desc) {}

        explicit operator bool() const {
            return type != Type::Undefined;
        }

        ImageDesc DeriveImage() const {
            Assert(type == Type::Image, "resource is not a render target");
            auto desc = imageDesc;
            desc.usage = {};
            return desc;
        }

        vk::Format ImageFormat() const {
            return imageDesc.format;
        }

        vk::Extent3D ImageExtents() const {
            return imageDesc.extent;
        }

        uint32 ImageLayers() const {
            return imageDesc.arrayLayers;
        }

        size_t BufferSize() const {
            return bufferDesc.layout.size;
        }

        ResourceID id = InvalidResource;
        Type type = Type::Undefined;

    private:
        union {
            ImageDesc imageDesc;
            BufferDesc bufferDesc;
        };

        friend class PassBuilder;
        friend class Resources;
        friend class RenderGraph;
    };

    class Resources {
    public:
        Resources(DeviceContext &device);

        PooledImagePtr TemporaryImage(const ImageDesc &desc);

        ImageViewPtr GetImageView(ResourceID id);
        ImageViewPtr GetImageView(string_view name);
        ImageViewPtr GetImageLayerView(ResourceID id, uint32 layer);
        ImageViewPtr GetImageLayerView(string_view name, uint32 layer);
        ImageViewPtr GetImageMipView(ResourceID id, uint32 mip);
        ImageViewPtr GetImageMipView(string_view name, uint32 mip);

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

    private:
        friend class RenderGraph;
        friend class PassBuilder;

        void ResizeIfNeeded();

        uint32 RefCount(ResourceID id);
        void IncrementRef(ResourceID id);
        void DecrementRef(ResourceID id);
        void AddUsageFromAccess(ResourceID id, Access access);

        ResourceID ReserveID(string_view name);
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
        vector<string> resourceNames;
        vector<ResourceID> freeIDs;
        size_t lastResourceCount = 0, consecutiveGrowthFrames = 0;

        vector<int32> refCounts;
        vector<PooledImagePtr> images;
        vector<BufferPtr> buffers;

        ResourceID lastOutputID = InvalidResource;

        PooledImagePtr GetPooledImage(ResourceID id);
        PooledImagePtr GetImageFromPool(const ImageDesc &desc);
        void TickImagePool();

        using PooledImageKey = HashKey<ImageDesc>;
        robin_hood::unordered_map<PooledImageKey, vector<PooledImagePtr>, typename PooledImageKey::Hasher> imagePool;
    };
} // namespace sp::vulkan::render_graph
