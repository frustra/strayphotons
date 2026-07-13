/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "Resources.hh"

#include "graphics/vulkan/core/DeviceContext.hh"
#include "strayphotons/Logging.hh"

#include <cstddef>
#include <memory>
#include <string_view>

namespace sp::vulkan::render_graph {
    Resources::Resources(DeviceContext &device) : device(device) {
        Reset();
        nameScopes.emplace_back(Scope{});
    }

    void Resources::Reset() {
        lastOutputID = InvalidResource;

        scopeStack.clear();
        scopeStack.push_back(0);

        for (auto &scope : nameScopes) {
            scope.frames[frameIndex].resourceNames.clear();
            scope.frames[frameIndex].passCount = 0;
        }

        sp::erase_if(externalIDs, [&](auto &id) {
            if (refCounts[id] == 1) DecrementRef(id);
            return refCounts[id] <= 0;
        });

        for (ResourceID id = 0; id < resources.size(); id++) {
            if (resources[id].type != Resource::Type::Undefined && refCounts[id] == 0) {
                Assert(!images[id], "dangling render target");
                Assert(!buffers[id], "dangling buffer");

                for (auto &scope : nameScopes) {
                    scope.ClearID(id);
                }

                freeIDs.push_back(id);
                resources[id].type = Resource::Type::Undefined;
                resourceNames[id] = {};
            }
        }
    }

    void Resources::AdvanceFrame() {
        frameIndex = (frameIndex + 1) % RESOURCE_FRAME_COUNT;
        Reset();

        if (resources.size() > lastResourceCount) {
            consecutiveGrowthFrames++;
        } else {
            consecutiveGrowthFrames = 0;
        }
        Assertf(consecutiveGrowthFrames < 100, "likely resource leak, have %d resources", resources.size());
        lastResourceCount = resources.size();

        TickImagePool();
    }

    void Resources::ResizeIfNeeded() {
        refCounts.resize(resources.size());
        images.resize(resources.size());
        buffers.resize(resources.size());
    }

    PooledImagePtr Resources::TemporaryImage(const ImageDesc &desc) {
        return GetImageFromPool(desc);
    }

    ImageViewPtr Resources::GetImageView(std::string_view name) {
        ResourceID id = GetID(name);
        Assertf(id != InvalidResource, "GetImageView resource does not exist: %s", name);
        return GetImageView(id);
    }

    ImageViewPtr Resources::GetImageView(ResourceID id) {
        if (id >= resources.size()) return nullptr;
        auto pooledImage = GetPooledImage(id);
        if (!pooledImage) return nullptr;
        return pooledImage->ImageView();
    }

    ImageViewPtr Resources::GetImageLayerView(std::string_view name, uint32_t layer) {
        ResourceID id = GetID(name);
        Assertf(id != InvalidResource, "GetImageLayerView resource does not exist: %s", name);
        return GetImageLayerView(id, layer);
    }

    ImageViewPtr Resources::GetImageLayerView(ResourceID id, uint32_t layer) {
        if (id >= resources.size()) return nullptr;
        auto pooledImage = GetPooledImage(id);
        if (!pooledImage) return nullptr;
        return pooledImage->LayerImageView(layer);
    }

    ImageViewPtr Resources::GetImageMipView(std::string_view name, uint32_t mip) {
        ResourceID id = GetID(name);
        Assertf(id != InvalidResource, "GetImageMipView resource does not exist: %s", name);
        return GetImageMipView(id, mip);
    }

    ImageViewPtr Resources::GetImageMipView(ResourceID id, uint32_t mip) {
        if (id >= resources.size()) return nullptr;
        auto pooledImage = GetPooledImage(id);
        if (!pooledImage) return nullptr;
        return pooledImage->MipImageView(mip);
    }

    ImageViewPtr Resources::GetImageDepthView(std::string_view name) {
        ResourceID id = GetID(name);
        Assertf(id != InvalidResource, "GetImageDepthView resource does not exist: %s", name);
        return GetImageDepthView(id);
    }

    ImageViewPtr Resources::GetImageDepthView(ResourceID id) {
        if (id >= resources.size()) return nullptr;
        auto pooledImage = GetPooledImage(id);
        if (!pooledImage) return nullptr;
        return pooledImage->DepthImageView();
    }

    PooledImagePtr Resources::GetPooledImage(ResourceID id) {
        if (renderThread == std::thread::id()) {
            renderThread = std::this_thread::get_id();
        } else {
            Assert(std::this_thread::get_id() == renderThread, "Resources must be used in a single thread");
        }

        Resource *res = GetResourcePtr(id);
        if (!res) return nullptr;
        Assertf(res->type == Resource::Type::Image, "resource %s is not a render target", resourceNames[res->id]);
        Assertf(RefCount(id) > 0, "can't get image %s without accessing it", resourceNames[res->id]);
        auto &target = images[res->id];
        if (!target) {
            if (res->imageDesc.usage == vk::ImageUsageFlagBits::eTransferDst) {
                Debugf("Image resource never accessed: %s", resourceNames[res->id]);
                return nullptr;
            }
            target = GetImageFromPool(res->imageDesc);
        }
        return target;
    }

    BufferPtr Resources::GetBuffer(std::string_view name) {
        ResourceID id = GetID(name);
        Assertf(id != InvalidResource, "GetBuffer resource does not exist: %s", name);
        return GetBuffer(id);
    }

    BufferPtr Resources::GetBuffer(ResourceID id) {
        Resource *res = GetResourcePtr(id);
        if (!res) return nullptr;
        Assertf(res->type == Resource::Type::Buffer, "resource %s is not a buffer", resourceNames[res->id]);
        Assertf(RefCount(id) > 0, "can't get buffer %s without accessing it", resourceNames[res->id]);
        auto &buf = buffers[res->id];
        if (!buf) {
            DebugAssertf(res->bufferDesc.usage != vk::BufferUsageFlags(),
                "resource %s has no usage flags",
                resourceNames[res->id]);
            buf = device.GetBuffer(res->bufferDesc);
        }
        DebugAssert(res->bufferDesc.usage == buf->Usage(), "buffer usage mismatch");
        return buf;
    }

    const Resource &Resources::GetResource(std::string_view name, bool followAliases) const {
        ResourceID id = GetID(name, 0);
        Assertf(id != InvalidResource, "resource does not exist: %s", name);
        return GetResource(id, followAliases);
    }

    const Resource &Resources::GetResource(ResourceID id, bool followAliases) const {
        Assertf(id < resources.size(), "resource id does not exist: %llu", id);
        if (followAliases) {
            ResourceID aliasID = id;
            while (aliasID < resources.size()) {
                id = aliasID;
                aliasID = resources[aliasID].AliasID();
            }
        }
        return resources[id];
    }

    Resource *Resources::GetResourcePtr(ResourceID id, bool followAliases) {
        if (id >= resources.size()) return nullptr;
        if (followAliases) {
            ResourceID aliasID = id;
            while (aliasID < resources.size()) {
                id = aliasID;
                aliasID = resources[aliasID].AliasID();
            }
            if (resources[id].type == Resource::Type::Alias) {
                return nullptr;
            }
        }
        return &resources[id];
    }

    const ResourceName &Resources::GetName(ResourceID id) const {
        if (id < resourceNames.size()) return resourceNames[id];
        static ResourceName invalidResourceName = "InvalidResource";
        return invalidResourceName;
    }

    ResourceID Resources::GetID(std::string_view name, uint32_t framesAgo) const {
        if (renderThread == std::thread::id()) {
            renderThread = std::this_thread::get_id();
        } else {
            Assert(std::this_thread::get_id() == renderThread, "Resources must be used in a single thread");
        }

        ResourceID result = InvalidResource;
        uint32_t getFrameIndex = (frameIndex + RESOURCE_FRAME_COUNT - framesAgo) % RESOURCE_FRAME_COUNT;

        auto lastSep = name.rfind('/');
        if (starts_with(name, "/")) {
            // The resource name is fully qualified, look it up directly.
            auto scopeName = name.substr(0, lastSep);
            auto resourceName = name.substr(lastSep + 1);

            for (auto &scope : nameScopes) {
                if (scope.name == scopeName) {
                    result = scope.GetID(resourceName, getFrameIndex);
                    break;
                }
            }
        } else {
            // The resource name is relative to the current or one of the parent scopes.
            std::string_view relativeScope;
            if (lastSep != name.npos) {
                relativeScope = name.substr(0, lastSep);
                name = name.substr(lastSep + 1);
            }

            for (auto scopeIt = scopeStack.rbegin(); scopeIt != scopeStack.rend(); scopeIt++) {
                if (!relativeScope.empty()) {
                    auto fullScopeName = nameScopes[*scopeIt].name + "/" + relativeScope;
                    for (auto &scope : nameScopes) {
                        if (scope.name == fullScopeName) {
                            result = scope.GetID(name, getFrameIndex);
                            break;
                        }
                    }
                    if (result != InvalidResource) return result;
                } else {
                    auto id = nameScopes[*scopeIt].GetID(name, getFrameIndex);
                    if (id != InvalidResource) return id;
                }
            }
        }
        return result;
    }

    ResourceID Resources::AddExternalImageView(std::string_view name, ImageViewPtr view, bool allowReplace) {
        if (renderThread == std::thread::id()) {
            renderThread = std::this_thread::get_id();
        } else {
            Assert(std::this_thread::get_id() == renderThread, "Resources must be used in a single thread");
        }

        Assertf(view, "Resources::AddExternalImageView called with null view");
        Assert(view->BaseArrayLayer() == 0, "RenderGraph::AddImageView can't target a specific layer");

        ImageDesc desc = {};
        desc.extent = view->Extent();
        desc.format = view->Format();
        desc.arrayLayers = view->ArrayLayers();

        if (!name.empty() && !allowReplace) {
            ResourceID existingID = GetID(name);
            if (existingID != InvalidResource) {
                const Resource &resource = GetResource(existingID, false);
                Assertf(resource.type == Resource::Type::Alias,
                    "Resources::AddExternalImageView called with existing name: %s",
                    name);
                // TODO: Make sure Alias references don't leak
            }
        }

        Resource resource(desc);
        resource.externalResource = true;
        if (Register(name, resource)) {
            externalIDs.emplace_back(resource.id);
            IncrementRef(resource.id);
            images[resource.id] = std::make_shared<PooledImage>(device, desc, view);
        }
        return resource.id;
    }

    uint32_t Resources::RefCount(ResourceID id) {
        Assert(id < resources.size(), "id out of range");
        return refCounts[id];
    }

    void Resources::IncrementRef(ResourceID id) {
        if (renderThread == std::thread::id()) {
            renderThread = std::this_thread::get_id();
        } else {
            Assert(std::this_thread::get_id() == renderThread, "Resources must be used in a single thread");
        }
        Assert(id < resources.size(), "id out of range");
        const Resource &res = resources[id];
        if (res.type == Resource::Type::Alias && res.aliasDesc.id < refCounts.size()) IncrementRef(res.aliasDesc.id);
        ResizeIfNeeded();
        ++refCounts[id];
    }

    void Resources::DecrementRef(ResourceID id) {
        if (renderThread == std::thread::id()) {
            renderThread = std::this_thread::get_id();
        } else {
            Assert(std::this_thread::get_id() == renderThread, "Resources must be used in a single thread");
        }
        Assert(id < resources.size(), "id out of range");
        const Resource &res = resources[id];
        if (res.type == Resource::Type::Alias && res.aliasDesc.id < refCounts.size()) DecrementRef(res.aliasDesc.id);
        if (--refCounts[id] > 0) return;

        switch (res.type) {
        case Resource::Type::Image:
            images[id].reset();
            break;
        case Resource::Type::Buffer:
            buffers[id].reset();
            break;
        case Resource::Type::Alias:
            break;
        default:
            Abortf("resource type is undefined: %s", resourceNames[id]);
        }
    }

    void Resources::AddUsageFromAccess(ResourceID id, Access access) {
        Assertf(id < resources.size(), "resource ID %u is invalid", id);
        Resource &res = resources[id];
        if (res.type == Resource::Type::Alias) {
            if (res.aliasDesc.id < resources.size()) {
                AddUsageFromAccess(res.aliasDesc.id, access);
            } else {
                res.aliasDesc.accessList.emplace_back(access);
            }
            return;
        }
        auto &acc = GetAccessInfo(access);
        switch (res.type) {
        case Resource::Type::Image:
            res.imageDesc.usage |= acc.imageUsageMask;
            break;
        case Resource::Type::Buffer:
            res.bufferDesc.usage |= acc.bufferUsageMask;
            break;
        default:
            Abortf("resource type is undefined: %s", resourceNames[id]);
        }
    }

    ResourceID Resources::ReserveID(std::string_view name) {
        Assert(!name.empty(), "Reserving empty render graph resource id");

        Resource futureResource(InvalidResource);
        Register(name, futureResource);
        return futureResource.id;
    }

    bool Resources::Register(std::string_view name, Resource &resource) {
        if (renderThread == std::thread::id()) {
            renderThread = std::this_thread::get_id();
        } else {
            Assert(std::this_thread::get_id() == renderThread, "Resources must be used in a single thread");
        }

        DebugZoneScoped;
        if (!name.empty()) {
            ResourceID existingID = InvalidResource;

            auto lastSep = name.rfind('/');
            if (starts_with(name, "/")) {
                // The resource name is fully qualified, look it up directly.
                auto scopeName = name.substr(0, lastSep);
                auto resourceName = name.substr(lastSep + 1);

                for (Scope &scope : nameScopes) {
                    if (scope.name == scopeName) {
                        existingID = scope.GetID(resourceName, frameIndex);
                        break;
                    }
                }
            } else {
                Assertf(lastSep == name.npos, "Resources::Register can't register name with relative scope: %s", name);

                // The resource name is relative to the current scope.
                existingID = nameScopes[scopeStack.back()].GetID(name, frameIndex);
            }

            if (existingID != InvalidResource) {
                Assertf(resourceNames[existingID] == name,
                    "Resource::Register %s resource already exists as %s",
                    name,
                    resourceNames[existingID]);
                Resource &res = resources[existingID];
                if (res.type == Resource::Type::Alias) {
                    Assertf(res.aliasDesc.id == InvalidResource,
                        "Resource::Register %s future resource defined twice",
                        name);
                } else {
                    Assertf(res.type == Resource::Type::Undefined,
                        "Resource::Register %s resource defined twice",
                        name);
                }
                resource.id = existingID;
                resources[existingID] = resource;
                return true;
            }
        }

        ResourceName scopeName;
        Scope *nameScope = nullptr;
        auto lastSep = name.rfind('/');
        if (starts_with(name, "/")) {
            // The resource name is fully qualified, look up the scope directly.
            scopeName = name.substr(0, lastSep);
            name = name.substr(lastSep + 1);

            for (auto &scope : nameScopes) {
                if (scope.name == scopeName) {
                    nameScope = &scope;
                    break;
                }
            }
        } else {
            // The resource name is relative to the current scope.
            nameScope = &nameScopes[scopeStack.back()];
        }
        if (!nameScope) return false;

        if (freeIDs.empty()) {
            resource.id = (ResourceID)resources.size();
            resources.push_back(resource);
            resourceNames.emplace_back(name);
        } else {
            resource.id = freeIDs.back();
            freeIDs.pop_back();
            resources[resource.id] = resource;
            resourceNames[resource.id] = name;
        }

        if (name.empty()) return false;

        nameScope->SetID(name, resource.id, frameIndex);
        return true;
    }

    void Resources::BeginScope(std::string_view name) {
        DebugZoneScoped;
        Assert(!name.empty(), "scopes must have a name");

        const auto &topScope = nameScopes[scopeStack.back()];
        ResourceName fullName = topScope.name + "/" + name;

        size_t scopeIndex = 0;
        for (size_t i = 0; i < nameScopes.size(); i++) {
            if (nameScopes[i].name == fullName) {
                scopeIndex = i;
                break;
            }
        }
        if (scopeIndex == 0) {
            scopeIndex = nameScopes.size();
            auto &newScope = nameScopes.emplace_back();
            newScope.name = std::move(fullName);
        }

        Assert(scopeStack.size() < MAX_RESOURCE_SCOPE_DEPTH, "too many nested scopes");
        scopeStack.push_back((uint8_t)scopeIndex);
    }

    void Resources::EndScope() {
        Assert(scopeStack.size() > 1, "tried to end a scope that wasn't started");
        const Scope &scope = nameScopes[scopeStack.back()];
        if (scope.frames[frameIndex].passCount > 0) {
            Scope &parentScope = nameScopes[scopeStack[scopeStack.size() - 2]];
            std::string_view resourceName = scope.name.substr(parentScope.name.size() + 1);
            ResourceID existingID = parentScope.GetID(resourceName, frameIndex);
            if (existingID != InvalidResource && existingID < resources.size()) {
                Resource &existing = resources[existingID];
                Assertf(existing.type == Resource::Type::Alias, "Expected existing scope root to be an Alias");
                ResizeIfNeeded();
                refCounts[lastOutputID] += refCounts[existingID];
                for (const Access &access : existing.aliasDesc.accessList) {
                    AddUsageFromAccess(lastOutputID, access);
                }
                existing.aliasDesc.accessList.clear();
                existing.aliasDesc.id = lastOutputID;
            } else {
                ResourceID aliasID = ReserveID(scope.name);
                Assertf(aliasID != InvalidResource, "Expected new scope root resource to be valid");
                Resource &res = resources[aliasID];
                Assertf(res.type == Resource::Type::Alias, "Expected new scope root to be an Alias");
                res.aliasDesc.id = lastOutputID;
            }
            parentScope.frames[frameIndex].passCount++;
        }
        scopeStack.pop_back();
    }

    ResourceID Resources::Scope::GetID(std::string_view name, uint32_t frameIndex) const {
        auto &resourceNames = frames[frameIndex].resourceNames;
        auto it = resourceNames.find(name);
        if (it != resourceNames.end()) return it->second;
        return InvalidResource;
    }

    void Resources::Scope::SetID(std::string_view name, ResourceID id, uint32_t frameIndex) {
        auto &resourceNames = frames[frameIndex].resourceNames;
        auto &nameID = resourceNames[name.data()];
        Assert(!nameID, "resource already registered");
        nameID = id;
    }

    void Resources::Scope::ClearID(ResourceID id) {
        for (auto &frame : frames) {
            auto it = frame.resourceNames.begin();
            while (it != frame.resourceNames.end()) {
                if (it->second == id) {
                    it = frame.resourceNames.erase(it);
                } else {
                    it++;
                }
            }
        }
    }

    PooledImagePtr Resources::GetImageFromPool(const ImageDesc &desc) {
        auto &list = imagePool[desc];
        for (auto &elemRef : list) {
            if (elemRef.use_count() <= 1 && elemRef->Desc() == desc) {
                elemRef->unusedFrames = 0;
                return elemRef;
            }
        }

        auto createDesc = desc;
        ZoneScopedN("CreatePooledImage");
        ZoneValue(imagePool.size());
        ZonePrintf("size=%dx%dx%d", createDesc.extent.width, desc.extent.height, desc.extent.depth);

        Assertf(createDesc.extent.width > 0 && desc.extent.height > 0 && desc.extent.depth > 0,
            "image must not have any zero extents, have %dx%dx%d",
            createDesc.extent.width,
            createDesc.extent.height,
            createDesc.extent.depth);

        if (createDesc.primaryViewType == vk::ImageViewType::e2D)
            createDesc.primaryViewType = createDesc.DeriveViewType();

        ImageCreateInfo imageInfo;
        imageInfo.imageType = createDesc.imageType;
        imageInfo.extent = createDesc.extent;
        imageInfo.mipLevels = createDesc.mipLevels;
        imageInfo.arrayLayers = createDesc.arrayLayers;
        imageInfo.format = createDesc.format;
        imageInfo.usage = createDesc.usage;

        ImageViewCreateInfo viewInfo = {};
        viewInfo.viewType = createDesc.primaryViewType;
        viewInfo.defaultSampler = device.GetSampler(createDesc.sampler);

        auto imageView = device.CreateImageAndView(imageInfo, viewInfo)->Get();
        auto ptr = std::make_shared<PooledImage>(device, createDesc, imageView);

        list.push_back(ptr);
        return ptr;
    }

    void Resources::TickImagePool() {
        for (auto it = imagePool.begin(); it != imagePool.end();) {
            auto &list = it->second;
            erase_if(list, [&](auto &elemRef) {
                if (elemRef.use_count() > 1) {
                    elemRef->unusedFrames = 0;
                    return false;
                }
                return elemRef->unusedFrames++ > 4;
            });
            if (list.empty()) {
                it = imagePool.erase(it);
            } else {
                it++;
            }
        }
    }
} // namespace sp::vulkan::render_graph
