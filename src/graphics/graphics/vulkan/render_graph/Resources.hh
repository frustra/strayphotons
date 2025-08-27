/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/FlatSet.hh"
#include "common/Hashing.hh"
#include "common/InlineVector.hh"
#include "graphics/vulkan/core/Access.hh"
#include "graphics/vulkan/core/Memory.hh"
#include "graphics/vulkan/core/VkCommon.hh"
#include "graphics/vulkan/render_graph/PooledImage.hh"

#include <robin_hood.h>
#include <thread>

namespace sp::vulkan::render_graph {
    typedef uint32_t ResourceID;
    typedef InlineString<127> ResourceName;

    const uint32_t MAX_RESOURCE_SCOPES = sizeof(uint8_t);
    const uint32_t MAX_RESOURCE_SCOPE_DEPTH = 4;
    const uint32_t RESOURCE_FRAME_COUNT = 2;

    const ResourceID InvalidResource = ~0u;

    struct Resource {
        enum class Type {
            Undefined,
            Image,
            Buffer,
            Future,
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

        uint32_t ImageLayers() const {
            return imageDesc.arrayLayers;
        }

        size_t BufferSize() const {
            return bufferDesc.layout.size;
        }

        ResourceID id = InvalidResource;
        Type type = Type::Undefined;
        bool externalResource = false;

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
        ImageViewPtr GetImageLayerView(ResourceID id, uint32_t layer);
        ImageViewPtr GetImageLayerView(string_view name, uint32_t layer);
        ImageViewPtr GetImageMipView(ResourceID id, uint32_t mip);
        ImageViewPtr GetImageMipView(string_view name, uint32_t mip);
        ImageViewPtr GetImageDepthView(ResourceID id);
        ImageViewPtr GetImageDepthView(string_view name);

        BufferPtr GetBuffer(ResourceID id);
        BufferPtr GetBuffer(string_view name);

        const Resource &GetResource(string_view name) const;
        const Resource &GetResource(ResourceID id) const;
        const ResourceName &GetName(ResourceID id) const;
        ResourceID GetID(string_view name, bool assertExists = true, uint32_t framesAgo = 0) const;

        ResourceID AddExternalImageView(string_view name, ImageViewPtr view, bool allowReplace = false);

        ResourceID LastOutputID() const {
            return lastOutputID;
        }
        const Resource &LastOutput() const {
            return GetResource(lastOutputID);
        }
        const ResourceName &LastOutputName() const {
            return GetName(lastOutputID);
        }

    private:
        friend class RenderGraph;
        friend class PassBuilder;

        void ResizeIfNeeded();

        uint32_t RefCount(ResourceID id);
        void IncrementRef(ResourceID id);
        void DecrementRef(ResourceID id);
        void AddUsageFromAccess(ResourceID id, Access access);

        ResourceID ReserveID(string_view name);
        bool Register(string_view name, Resource &resource);

        void BeginScope(string_view name);
        void EndScope();

        void AdvanceFrame();
        void Reset();

        Resource &GetResourceRef(ResourceID id) {
            Assertf(id < resources.size(), "resource ID %u is invalid", id);
            return resources[id];
        }

        DeviceContext &device;
        uint32_t frameIndex = 0;
        mutable std::thread::id renderThread;

        struct Scope {
            ResourceName name;

            struct PerFrame {
                // robin_hood::unordered_flat_map<ResourceName, ResourceID, StringHash, StringEqual> resourceNames;
                std::unordered_map<ResourceName, ResourceID, StringHash, StringEqual> resourceNames;
            };
            std::array<PerFrame, RESOURCE_FRAME_COUNT> frames = {};

            ResourceID GetID(string_view name, uint32_t frameIndex) const;
            void SetID(string_view name, ResourceID id, uint32_t frameIndex, bool replace = false);
            void ClearID(ResourceID id);
        };

        vector<Scope> nameScopes;
        InlineVector<uint8_t, MAX_RESOURCE_SCOPE_DEPTH> scopeStack; // refers to indexes in nameScopes

        vector<Resource> resources;
        vector<ResourceName> resourceNames;
        vector<ResourceID> freeIDs;
        vector<ResourceID> externalIDs;
        size_t lastResourceCount = 0, consecutiveGrowthFrames = 0;

        vector<int32_t> refCounts;
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
