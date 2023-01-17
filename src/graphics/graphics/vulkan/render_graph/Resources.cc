#include "Resources.hh"

#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::render_graph {
    Resources::Resources(DeviceContext &device) : device(device) {
        Reset();
        nameScopes.emplace_back();
    }

    void Resources::Reset() {
        lastOutputID = InvalidResource;

        scopeStack.clear();
        scopeStack.push_back(0);

        for (auto &scope : nameScopes) {
            scope.frames[frameIndex].resourceNames.clear();
        }

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

    ImageViewPtr Resources::GetImageView(string_view name) {
        return GetImageView(GetID(name));
    }

    ImageViewPtr Resources::GetImageView(ResourceID id) {
        if (id >= resources.size()) return nullptr;
        return GetPooledImage(id)->ImageView();
    }

    ImageViewPtr Resources::GetImageLayerView(string_view name, uint32 layer) {
        return GetImageLayerView(GetID(name), layer);
    }

    ImageViewPtr Resources::GetImageLayerView(ResourceID id, uint32 layer) {
        if (id >= resources.size()) return nullptr;
        return GetPooledImage(id)->LayerImageView(layer);
    }

    ImageViewPtr Resources::GetImageMipView(string_view name, uint32 mip) {
        return GetImageMipView(GetID(name), mip);
    }

    ImageViewPtr Resources::GetImageMipView(ResourceID id, uint32 mip) {
        if (id >= resources.size()) return nullptr;
        return GetPooledImage(id)->MipImageView(mip);
    }

    ImageViewPtr Resources::GetImageDepthView(string_view name) {
        return GetImageDepthView(GetID(name));
    }

    ImageViewPtr Resources::GetImageDepthView(ResourceID id) {
        if (id >= resources.size()) return nullptr;
        return GetPooledImage(id)->DepthImageView();
    }

    PooledImagePtr Resources::GetPooledImage(ResourceID id) {
        if (id >= resources.size()) return nullptr;
        auto &res = resources[id];
        Assertf(res.type == Resource::Type::Image, "resource %s is not a render target", resourceNames[id]);
        Assertf(RefCount(id) > 0, "can't get image %s without accessing it", resourceNames[id]);
        auto &target = images[res.id];
        if (!target) target = GetImageFromPool(res.imageDesc);
        return target;
    }

    BufferPtr Resources::GetBuffer(string_view name) {
        return GetBuffer(GetID(name));
    }

    BufferPtr Resources::GetBuffer(ResourceID id) {
        if (id >= resources.size()) return nullptr;
        auto &res = resources[id];
        Assertf(res.type == Resource::Type::Buffer, "resource %s is not a buffer", resourceNames[id]);
        Assertf(RefCount(id) > 0, "can't get buffer %s without accessing it", resourceNames[id]);
        auto &buf = buffers[res.id];
        if (!buf) {
            DebugAssertf(res.bufferDesc.usage != vk::BufferUsageFlags(),
                "resource %s has no usage flags",
                resourceNames[id]);
            buf = device.GetBuffer(res.bufferDesc);
        }
        DebugAssert(res.bufferDesc.usage == buf->Usage(), "buffer usage mismatch");
        return buf;
    }

    const Resource &Resources::GetResource(string_view name) const {
        return GetResource(GetID(name, false));
    }

    const Resource &Resources::GetResource(ResourceID id) const {
        if (id < resources.size()) return resources[id];
        static Resource invalidResource = {};
        return invalidResource;
    }

    ResourceID Resources::GetID(string_view name, bool assertExists, int framesAgo) const {
        ResourceID result = InvalidResource;
        uint32 getFrameIndex = (frameIndex + RESOURCE_FRAME_COUNT - framesAgo) % RESOURCE_FRAME_COUNT;

        if (name.find(':') == string_view::npos && name.find('.') != string_view::npos) {
            // Any resource name with a dot is assumed to be fully qualified.
            auto lastDot = name.rfind('.');
            auto scopeName = name.substr(0, lastDot);
            auto resourceName = name.substr(lastDot + 1);

            for (auto &scope : nameScopes) {
                if (scope.name == scopeName) {
                    result = scope.GetID(resourceName, getFrameIndex);
                    break;
                }
            }
            Assertf(!assertExists || result != InvalidResource, "resource does not exist: %s", name);
            return result;
        }

        for (auto scopeIt = scopeStack.rbegin(); scopeIt != scopeStack.rend(); scopeIt++) {
            auto id = nameScopes[*scopeIt].GetID(name, getFrameIndex);
            if (id != InvalidResource) return id;
        }
        Assertf(!assertExists, "resource does not exist: %s", name);
        return InvalidResource;
    }

    uint32 Resources::RefCount(ResourceID id) {
        Assert(id < resources.size(), "id out of range");
        return refCounts[id];
    }

    void Resources::IncrementRef(ResourceID id) {
        Assert(id < resources.size(), "id out of range");
        ResizeIfNeeded();
        ++refCounts[id];
    }

    void Resources::DecrementRef(ResourceID id) {
        Assert(id < resources.size(), "id out of range");
        if (--refCounts[id] > 0) return;

        auto &res = resources[id];
        switch (res.type) {
        case Resource::Type::Image:
            images[id].reset();
            break;
        case Resource::Type::Buffer:
            buffers[id].reset();
            break;
        default:
            Abort("resource type is undefined");
        }
    }

    void Resources::AddUsageFromAccess(ResourceID id, Access access) {
        auto &res = GetResourceRef(id);
        auto &acc = GetAccessInfo(access);
        if (res.type == Resource::Type::Image) {
            res.imageDesc.usage |= acc.imageUsageMask;
        } else if (res.type == Resource::Type::Buffer) {
            res.bufferDesc.usage |= acc.bufferUsageMask;
        }
    }

    ResourceID Resources::ReserveID(string_view name) {
        if (name.empty()) return InvalidResource;

        Resource futureResource;
        Register(name, futureResource);
        return futureResource.id;
    }

    void Resources::Register(string_view name, Resource &resource) {
        if (!name.empty()) {
            auto existingID = GetID(name, false);
            if (existingID != InvalidResource) {
                Assert(resourceNames[existingID] == name, "resource defined with a different name");
                Assert(resources[existingID].type == Resource::Type::Undefined, "resource defined twice");
                resource.id = existingID;
                resources[existingID] = resource;
                return;
            }
        }

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

        if (name.empty()) return;
        nameScopes[scopeStack.back()].SetID(name, resource.id, frameIndex);
    }

    void Resources::BeginScope(string_view name) {
        Assert(!name.empty(), "scopes must have a name");

        const auto &topScope = nameScopes[scopeStack.back()];
        string fullName;

        if (topScope.name.empty()) {
            fullName = name;
        } else {
            fullName = topScope.name + ".";
            fullName.append(name);
        }

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
        scopeStack.push_back((uint8)scopeIndex);
    }

    void Resources::EndScope() {
        Assert(scopeStack.size() > 1, "tried to end a scope that wasn't started");
        auto &scope = nameScopes[scopeStack.back()];
        scope.SetID("LastOutput", LastOutputID(), frameIndex);
        scopeStack.pop_back();
    }

    ResourceID Resources::Scope::GetID(string_view name, uint32 frameIndex) const {
        auto &resourceNames = frames[frameIndex].resourceNames;
        auto it = resourceNames.find(name);
        if (it != resourceNames.end()) return it->second;
        return InvalidResource;
    }

    void Resources::Scope::SetID(string_view name, ResourceID id, uint32 frameIndex) {
        auto &resourceNames = frames[frameIndex].resourceNames;
        Assert(name.data()[name.size()] == '\0', "string_view is not null terminated");
        auto &nameID = resourceNames[name.data()];
        Assert(!nameID, "resource already registered");
        nameID = id;
    }

    void Resources::Scope::ClearID(ResourceID id) {
        for (auto &frame : frames) {
            auto it = frame.resourceNames.begin();
            while (it != frame.resourceNames.end()) {
                if (it->second == id) {
                    frame.resourceNames.erase(it++);
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

        ImageViewCreateInfo viewInfo;
        viewInfo.viewType = createDesc.primaryViewType;
        viewInfo.defaultSampler = device.GetSampler(createDesc.sampler);

        auto imageView = device.CreateImageAndView(imageInfo, viewInfo)->Get();
        auto ptr = make_shared<PooledImage>(device, createDesc, imageView);

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
