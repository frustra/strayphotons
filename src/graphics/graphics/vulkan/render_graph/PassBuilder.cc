#include "PassBuilder.hh"

namespace sp::vulkan::render_graph {
    const Resource &PassBuilder::TransferRead(string_view name) {
        return TransferRead(GetID(name));
    }

    const Resource &PassBuilder::TransferRead(ResourceID id) {
        auto &resource = resources.GetResourceRef(id);
        ResourceAccess access = {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferRead};
        if (resource.type == Resource::Type::RenderTarget) {
            resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eTransferSrc;
            access.layout = vk::ImageLayout::eTransferSrcOptimal;
        } else if (resource.type == Resource::Type::Buffer) {
            resource.bufferDesc.usage |= vk::BufferUsageFlagBits::eTransferSrc;
        }
        pass.AddDependency(access, resource);
        return resource;
    }

    const Resource &PassBuilder::TransferWrite(string_view name) {
        return TransferWrite(GetID(name));
    }

    const Resource &PassBuilder::TransferWrite(ResourceID id) {
        auto &resource = resources.GetResourceRef(id);
        ResourceAccess access = {vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite};
        if (resource.type == Resource::Type::RenderTarget) {
            resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eTransferDst;
            access.layout = vk::ImageLayout::eTransferDstOptimal;
        } else if (resource.type == Resource::Type::Buffer) {
            resource.bufferDesc.usage |= vk::BufferUsageFlagBits::eTransferDst;
        }
        pass.AddDependency(access, resource);
        return resource;
    }
    const Resource &PassBuilder::UniformRead(string_view name, PipelineStageMask stages) {
        return UniformRead(GetID(name), stages);
    }

    const Resource &PassBuilder::UniformRead(ResourceID id, PipelineStageMask stages) {
        auto &resource = resources.GetResourceRef(id);
        ResourceAccess access = {
            stages,
            vk::AccessFlagBits::eUniformRead,
        };
        resource.bufferDesc.usage |= vk::BufferUsageFlagBits::eUniformBuffer;
        resource.bufferDesc.residency = Residency::CPU_TO_GPU;
        pass.AddDependency(access, resource);
        return resource;
    }

    const Resource &PassBuilder::TextureRead(string_view name, PipelineStageMask stages) {
        return TextureRead(GetID(name), stages);
    }

    const Resource &PassBuilder::TextureRead(ResourceID id, PipelineStageMask stages) {
        auto &resource = resources.GetResourceRef(id);
        resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eSampled;

        auto aspect = FormatToAspectFlags(resource.renderTargetDesc.format);
        bool depth = !!(aspect & vk::ImageAspectFlagBits::eDepth);
        bool stencil = !!(aspect & vk::ImageAspectFlagBits::eStencil);

        auto layout = vk::ImageLayout::eShaderReadOnlyOptimal;
        if (depth && stencil)
            layout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
        else if (depth)
            layout = vk::ImageLayout::eDepthReadOnlyOptimal;
        else if (stencil)
            layout = vk::ImageLayout::eStencilReadOnlyOptimal;

        ResourceAccess access = {stages, vk::AccessFlagBits::eShaderRead, layout};
        pass.AddDependency(access, resource);
        return resource;
    }

    const Resource &PassBuilder::StorageRead(string_view name, PipelineStageMask stages) {
        return StorageRead(GetID(name), stages);
    }

    const Resource &PassBuilder::StorageRead(ResourceID id, PipelineStageMask stages) {
        auto &resource = resources.GetResourceRef(id);
        ResourceAccess access = {
            stages,
            vk::AccessFlagBits::eShaderRead,
        };
        if (resource.type == Resource::Type::RenderTarget) {
            resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eStorage;
            access.layout = vk::ImageLayout::eGeneral;
        } else if (resource.type == Resource::Type::Buffer) {
            resource.bufferDesc.usage |= vk::BufferUsageFlagBits::eStorageBuffer;
        }
        pass.AddDependency(access, resource);
        return resource;
    }

    const Resource &PassBuilder::StorageWrite(string_view name, PipelineStageMask stages) {
        return StorageWrite(GetID(name), stages);
    }

    const Resource &PassBuilder::StorageWrite(ResourceID id, PipelineStageMask stages) {
        auto &resource = resources.GetResourceRef(id);
        ResourceAccess access = {
            stages,
            vk::AccessFlagBits::eShaderWrite,
        };
        if (resource.type == Resource::Type::RenderTarget) {
            resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eStorage;
            access.layout = vk::ImageLayout::eGeneral;
        } else if (resource.type == Resource::Type::Buffer) {
            resource.bufferDesc.usage |= vk::BufferUsageFlagBits::eStorageBuffer;
        }
        pass.AddDependency(access, resource);
        return resource;
    }

    const Resource &PassBuilder::BufferAccess(string_view name,
        PipelineStageMask stages,
        AccessMask access,
        BufferUsageMask usage) {
        return BufferAccess(GetID(name), stages, access, usage);
    }

    const Resource &PassBuilder::BufferAccess(ResourceID id,
        PipelineStageMask stages,
        AccessMask accesses,
        BufferUsageMask usage) {
        auto &resource = resources.GetResourceRef(id);
        ResourceAccess access = {
            stages,
            accesses,
        };
        resource.bufferDesc.usage |= usage;
        pass.AddDependency(access, resource);
        return resource;
    }

    Resource PassBuilder::RenderTargetCreate(string_view name, const RenderTargetDesc &desc) {
        Resource resource(desc);
        resources.Register(name, resource);
        pass.AddOutput(resource.id);
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
        auto &res = resources.GetResourceRef(id);
        Assert(res.type == Resource::Type::RenderTarget, "resource must be a render target");
        res.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
        ResourceAccess access = {
            vk::PipelineStageFlagBits::eEarlyFragmentTests,
            vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            vk::ImageLayout::eDepthStencilAttachmentOptimal,
        };
        pass.AddDependency(access, res);
        SetAttachment(MAX_COLOR_ATTACHMENTS, id, info);
    }

    Resource PassBuilder::OutputDepthAttachment(string_view name, RenderTargetDesc desc, const AttachmentInfo &info) {
        desc.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
        return OutputAttachment(MAX_COLOR_ATTACHMENTS, name, desc, info);
    }

    void PassBuilder::SetPrimaryAttachment(uint32 index) {
        Assert(index < pass.attachments.size(), "index must point to a valid attachment");
        pass.primaryAttachmentIndex = index;
    }

    Resource PassBuilder::BufferCreate(size_t size, BufferUsageMask usage, Residency residency) {
        return BufferCreate("", size, usage, residency);
    }

    Resource PassBuilder::BufferCreate(string_view name, size_t size, BufferUsageMask usage, Residency residency) {
        BufferDesc desc;
        desc.size = size;
        desc.usage = usage;
        desc.residency = residency;
        Resource resource(desc);
        resources.Register(name, resource);
        pass.AddOutput(resource.id);
        return resource;
    }

    Resource PassBuilder::OutputAttachment(uint32 index,
        string_view name,
        const RenderTargetDesc &desc,
        const AttachmentInfo &info) {
        auto resource = RenderTargetCreate(name, desc);
        SetAttachmentWithoutOutput(index, resource.id, info);
        return resource;
    }

    void PassBuilder::SetAttachment(uint32 index, ResourceID id, const AttachmentInfo &info) {
        pass.AddOutput(id);
        SetAttachmentWithoutOutput(index, id, info);
    }

    void PassBuilder::SetAttachmentWithoutOutput(uint32 index, ResourceID id, const AttachmentInfo &info) {
        auto &attachment = pass.attachments[index];
        attachment = info;
        attachment.resourceID = id;
    }
} // namespace sp::vulkan::render_graph
