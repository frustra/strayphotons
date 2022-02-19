#include "PassBuilder.hh"

namespace sp::vulkan::render_graph {
    const Resource &PassBuilder::ShaderRead(string_view name) {
        return ShaderRead(resources.GetID(name));
    }

    const Resource &PassBuilder::ShaderRead(ResourceID id) {
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

        ResourceAccess access = {vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead, layout};
        pass.AddDependency(access, resource);
        return resource;
    }

    const Resource &PassBuilder::TransferRead(string_view name) {
        return TransferRead(resources.GetID(name));
    }

    const Resource &PassBuilder::TransferRead(ResourceID id) {
        auto &resource = resources.GetResourceRef(id);
        resource.renderTargetDesc.usage |= vk::ImageUsageFlagBits::eTransferSrc;
        ResourceAccess access = {
            vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferRead,
            vk::ImageLayout::eTransferSrcOptimal,
        };
        pass.AddDependency(access, resource);
        return resource;
    }

    Resource PassBuilder::CreateRenderTarget(string_view name, const RenderTargetDesc &desc) {
        Resource resource(desc);
        resources.Register(name, resource);
        pass.AddOutput(resource.id);
        return resource;
    }

    void PassBuilder::SetColorAttachment(uint32 index, string_view name, const AttachmentInfo &info) {
        SetColorAttachment(index, resources.GetID(name), info);
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
        SetDepthAttachment(resources.GetID(name), info);
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

    Resource PassBuilder::CreateBuffer(BufferType bufferType, size_t size) {
        return CreateBuffer(bufferType, "", size);
    }

    Resource PassBuilder::CreateBuffer(BufferType bufferType, string_view name, size_t size) {
        Resource resource(bufferType, size);
        resources.Register(name, resource);
        pass.AddOutput(resource.id);
        return resource;
    }

    const Resource &PassBuilder::ReadBuffer(string_view name) {
        return ReadBuffer(resources.GetID(name));
    }

    const Resource &PassBuilder::ReadBuffer(ResourceID id) {
        auto &resource = resources.GetResourceRef(id);
        // TODO: need to mark stages and usage for buffers that are written from the GPU,
        // so we can generate barriers. For now this is only used for CPU->GPU.
        pass.AddDependency({}, resource);
        return resource;
    }

    Resource PassBuilder::OutputAttachment(uint32 index,
        string_view name,
        const RenderTargetDesc &desc,
        const AttachmentInfo &info) {
        auto resource = CreateRenderTarget(name, desc);
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
