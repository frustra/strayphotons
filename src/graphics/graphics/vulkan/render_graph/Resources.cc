#include "Resources.hh"

#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::render_graph {
    void Resources::Reset() {
        lastOutputID = InvalidResource;

        scopeStack.clear();
        scopeStack.push_back(0);

        for (auto &scope : nameScopes) {
            scope.frames[frameIndex].resourceNames.clear();
        }

        for (ResourceID id = 0; id < resources.size(); id++) {
            if (resources[id].type != Resource::Type::Undefined && refCounts[id] == 0) {
                Assert(!renderTargets[id], "dangling render target");
                Assert(!buffers[id], "dangling buffer");

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
    }

    void Resources::ResizeIfNeeded() {
        refCounts.resize(resources.size());
        renderTargets.resize(resources.size());
        buffers.resize(resources.size());
        lastResourceAccess.resize(resources.size());
    }

    RenderTargetPtr Resources::GetRenderTarget(string_view name) {
        return GetRenderTarget(GetID(name));
    }

    RenderTargetPtr Resources::GetRenderTarget(ResourceID id) {
        if (id >= resources.size()) return nullptr;
        auto &res = resources[id];
        Assert(res.type == Resource::Type::RenderTarget, "resource is not a render target");
        auto &target = renderTargets[res.id];
        if (!target) target = device.GetRenderTarget(res.renderTargetDesc);
        return target;
    }

    BufferPtr Resources::GetBuffer(string_view name) {
        return GetBuffer(GetID(name));
    }

    BufferPtr Resources::GetBuffer(ResourceID id) {
        if (id >= resources.size()) return nullptr;
        auto &res = resources[id];
        Assert(res.type == Resource::Type::Buffer, "resource is not a buffer");
        auto &buf = buffers[res.id];
        if (!buf) buf = device.GetBuffer(res.bufferDesc);
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

        if (name.find('.') != string_view::npos) {
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
            Assert(!assertExists || result != InvalidResource, string("resource does not exist: ").append(name));
            return result;
        }

        for (auto scopeIt = scopeStack.rbegin(); scopeIt != scopeStack.rend(); scopeIt++) {
            auto id = nameScopes[*scopeIt].GetID(name, getFrameIndex);
            if (id != InvalidResource) return id;
        }
        Assert(!assertExists, string("resource does not exist: ").append(name));
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
        case Resource::Type::RenderTarget:
            renderTargets[id].reset();
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
        auto &acc = AccessMap[(size_t)access];
        if (res.type == Resource::Type::RenderTarget) {
            res.renderTargetDesc.usage |= acc.imageUsageMask;
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
} // namespace sp::vulkan::render_graph
