#include "PassBuilder.hh"

namespace sp::vulkan::render_graph {
    void PassBuilder::Read(ResourceID id, Access access) {
        pass.AddAccess(id, access);
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
        if (prevFrameID != InvalidResource) pass.AddAccess(prevFrameID, access);
        return prevFrameID;
    }

    void PassBuilder::Write(ResourceID id, Access access) {
        pass.AddAccess(id, access);
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

    Resource PassBuilder::CreateImage(string_view name, const ImageDesc &desc, Access access) {
        Resource resource(desc);
        resources.Register(name, resource);
        pass.AddAccess(resource.id, access);
        return resource;
    }

    void PassBuilder::SetColorAttachment(uint32 index, string_view name, const AttachmentInfo &info) {
        SetColorAttachment(index, GetID(name), info);
    }

    void PassBuilder::SetColorAttachment(uint32 index, ResourceID id, const AttachmentInfo &info) {
        auto &res = resources.GetResourceRef(id);
        Assert(res.type == Resource::Type::Image, "resource must be a render target");
        Write(id, Access::ColorAttachmentReadWrite);
        SetAttachment(index, id, info);
    }

    Resource PassBuilder::OutputColorAttachment(uint32 index,
        string_view name,
        ImageDesc desc,
        const AttachmentInfo &info) {
        return OutputAttachment(index, name, desc, info);
    }

    void PassBuilder::SetDepthAttachment(string_view name, const AttachmentInfo &info) {
        SetDepthAttachment(GetID(name), info);
    }

    void PassBuilder::SetDepthAttachment(ResourceID id, const AttachmentInfo &info) {
        Write(id, Access::DepthStencilAttachmentWrite);
        SetAttachment(MAX_COLOR_ATTACHMENTS, id, info);
    }

    Resource PassBuilder::OutputDepthAttachment(string_view name, ImageDesc desc, const AttachmentInfo &info) {
        auto resource = CreateImage(name, desc, Access::DepthStencilAttachmentWrite);
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
        desc.residency = residency;
        Resource resource(desc);
        resources.Register(name, resource);
        pass.AddAccess(resource.id, access);
        return resource;
    }

    Resource PassBuilder::OutputAttachment(uint32 index,
        string_view name,
        const ImageDesc &desc,
        const AttachmentInfo &info) {
        auto resource = CreateImage(name, desc, Access::ColorAttachmentWrite);
        SetAttachment(index, resource.id, info);
        return resource;
    }

    void PassBuilder::SetAttachment(uint32 index, ResourceID id, const AttachmentInfo &info) {
        auto &attachment = pass.attachments[index];
        attachment = info;
        attachment.resourceID = id;
    }
} // namespace sp::vulkan::render_graph
