#include "Resources.hh"

#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan::render_graph {
    void Resources::Reset() {
        nameScopes.clear();
        nameScopes.emplace_back();
        scopeStack.clear();
        scopeStack.push_back(0);
        resources.clear();
        refCounts.clear();
        renderTargets.clear();
        buffers.clear();
        lastOutputID = InvalidResource;
    }

    void Resources::ResizeBeforeExecute() {
        refCounts.resize(resources.size());
        renderTargets.resize(resources.size());
        buffers.resize(resources.size());
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
        if (!buf) buf = device.GetFramePooledBuffer(res.bufferDesc.type, res.bufferDesc.size);
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

    ResourceID Resources::GetID(string_view name, bool assertExists) const {
        ResourceID result = InvalidResource;

        if (name.find('.') != string_view::npos) {
            // Any resource name with a dot is assumed to be fully qualified.
            auto lastDot = name.rfind('.');
            auto scopeName = name.substr(0, lastDot);
            auto resourceName = name.substr(lastDot + 1);

            for (auto &scope : nameScopes) {
                if (scope.name == scopeName) {
                    result = scope.GetID(resourceName);
                    break;
                }
            }
            Assert(!assertExists || result == InvalidResource, string("resource does not exist: ").append(name));
            return result;
        }

        for (auto scopeIt = scopeStack.rbegin(); scopeIt != scopeStack.rend(); scopeIt++) {
            auto id = nameScopes[*scopeIt].GetID(name);
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

    void Resources::Register(string_view name, Resource &resource) {
        resource.id = (ResourceID)resources.size();
        resources.push_back(resource);

        if (name.empty()) return;
        nameScopes[scopeStack.back()].SetID(name, resource.id);
    }

    void Resources::BeginScope(string_view name) {
        auto &newScope = nameScopes.emplace_back();
        const auto &topScope = nameScopes[scopeStack.back()];

        if (topScope.name.empty()) {
            newScope.name = name;
        } else {
            newScope.name = topScope.name + ".";
            newScope.name.append(name);
        }

        Assert(scopeStack.size() < MAX_RESOURCE_SCOPE_DEPTH, "too many nested scopes");
        scopeStack.push_back((uint8)(nameScopes.size() - 1));
    }

    void Resources::EndScope() {
        Assert(scopeStack.size() > 1, "tried to end a scope that wasn't started");
        auto &scope = nameScopes[scopeStack.back()];
        if (scope.resourceNames.size() > 0) scope.SetID("LastOutput", LastOutputID());
        scopeStack.pop_back();
    }

    ResourceID Resources::Scope::GetID(string_view name) const {
        auto it = resourceNames.find(name);
        if (it != resourceNames.end()) return it->second;
        return InvalidResource;
    }

    void Resources::Scope::SetID(string_view name, ResourceID id) {
        Assert(name.data()[name.size()] == '\0', "string_view is not null terminated");
        auto &nameID = resourceNames[name.data()];
        Assert(!nameID, "resource already registered");
        nameID = id;
    }

} // namespace sp::vulkan::render_graph
