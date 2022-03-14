#include "PassBuilder.hh"

namespace sp::vulkan::render_graph {
    void PassBuilder::Read(ResourceID id, Access access) {
        pass.AddRead(id, access);
    }

    ResourceID PassBuilder::Read(string_view name, Access access) {
        auto id = GetID(name);
        Read(id, access);
        return id;
    }

    ResourceID PassBuilder::ReadPreviousFrame(string_view name, Access access, int framesAgo) {
        auto thisFrameID = resources.GetID(name, false);
        if (thisFrameID == InvalidResource) thisFrameID = resources.ReserveID(name);
        pass.AddFutureRead(thisFrameID, access, framesAgo);

        auto prevFrameID = resources.GetID(name, false, framesAgo);
        if (prevFrameID != InvalidResource) pass.AddRead(prevFrameID, access);
        return prevFrameID;
    }

    void PassBuilder::Write(ResourceID id, Access access) {
        pass.AddWrite(id, access);
    }

    ResourceID PassBuilder::Write(string_view name, Access access) {
        auto id = GetID(name);
        Write(id, access);
        return id;
    }

    const Resource &PassBuilder::ReadUniform(string_view name) {
        return ReadUniform(GetID(name));
    }

    const Resource &PassBuilder::ReadUniform(ResourceID id) {
        Read(id, Access::AnyShaderReadUniform);
        auto &resource = resources.GetResourceRef(id);
        resource.bufferDesc.residency = Residency::CPU_TO_GPU;
        return resource;
    }

    Resource PassBuilder::CreateRenderTarget(string_view name, const RenderTargetDesc &desc, Access access) {
        Resource resource(desc);
        resources.Register(name, resource);
        pass.AddCreate(resource.id, access);
        return resource;
    }

    void PassBuilder::SetColorAttachment(uint32 index, string_view name, const AttachmentInfo &info) {
        SetColorAttachment(index, GetID(name), info);
    }

    void PassBuilder::SetColorAttachment(uint32 index, ResourceID id, const AttachmentInfo &info) {
        auto &res = resources.GetResourceRef(id);
        Assert(res.type == Resource::Type::RenderTarget, "resource must be a render target");
        res.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eColorAttachment;
        SetAttachment(index, id, info);
    }

    Resource PassBuilder::OutputColorAttachment(uint32 index,
        string_view name,
        RenderTargetDesc desc,
        const AttachmentInfo &info) {
        desc.usage |= vk::ImageUsageFlagBits::eColorAttachment;
        return OutputAttachment(index, name, desc, info);
    }

    void PassBuilder::SetDepthAttachment(string_view name, const AttachmentInfo &info) {
        SetDepthAttachment(GetID(name), info);
    }

    void PassBuilder::SetDepthAttachment(ResourceID id, const AttachmentInfo &info) {
        Write(id, Access::DepthStencilAttachmentWrite);
        SetAttachment(MAX_COLOR_ATTACHMENTS, id, info);
    }

    Resource PassBuilder::OutputDepthAttachment(string_view name, RenderTargetDesc desc, const AttachmentInfo &info) {
        auto resource = CreateRenderTarget(name, desc, Access::DepthStencilAttachmentWrite);
        SetAttachment(MAX_COLOR_ATTACHMENTS, resource.id, info);
        return resource;
    }

    void PassBuilder::SetPrimaryAttachment(uint32 index) {
        Assert(index < pass.attachments.size(), "index must point to a valid attachment");
        pass.primaryAttachmentIndex = index;
    }

    Resource PassBuilder::CreateBuffer(size_t size, Residency residency, Access access) {
        return CreateBuffer("", size, residency, access);
    }

    Resource PassBuilder::CreateBuffer(string_view name, size_t size, Residency residency, Access access) {
        BufferDesc desc;
        desc.size = size;
        desc.usage |= AccessMap[(size_t)access].bufferUsageMask;
        desc.residency = residency;
        Resource resource(desc);
        resources.Register(name, resource);
        pass.AddCreate(resource.id, access);
        return resource;
    }

    Resource PassBuilder::OutputAttachment(uint32 index,
        string_view name,
        const RenderTargetDesc &desc,
        const AttachmentInfo &info) {
        auto resource = CreateRenderTarget(name, desc, Access::ColorAttachmentWrite);
        SetAttachment(index, resource.id, info);
        return resource;
    }

    void PassBuilder::SetAttachment(uint32 index, ResourceID id, const AttachmentInfo &info) {
        auto &attachment = pass.attachments[index];
        attachment = info;
        attachment.resourceID = id;
    }
} // namespace sp::vulkan::render_graph
