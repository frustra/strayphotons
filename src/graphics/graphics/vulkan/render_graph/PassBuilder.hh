#pragma once

#include "graphics/vulkan/render_graph/Pass.hh"
#include "graphics/vulkan/render_graph/Resources.hh"

namespace sp::vulkan::render_graph {
    class PassBuilder {
    public:
        PassBuilder(Resources &resources, Pass &pass) : resources(resources), pass(pass) {}

        Resource GetResource(ResourceID id) {
            return resources.GetResource(id);
        }
        Resource GetResource(string_view name) {
            return resources.GetResource(name);
        }

        const Resource &ShaderRead(string_view name);
        const Resource &ShaderRead(ResourceID id);

        const Resource &TransferRead(string_view name);
        const Resource &TransferRead(ResourceID id);

        void SetColorAttachment(uint32 index, string_view name, const AttachmentInfo &info);
        void SetColorAttachment(uint32 index, ResourceID id, const AttachmentInfo &info);
        Resource OutputColorAttachment(uint32 index,
            string_view name,
            RenderTargetDesc desc,
            const AttachmentInfo &info);

        void SetDepthAttachment(string_view name, const AttachmentInfo &info);
        void SetDepthAttachment(ResourceID id, const AttachmentInfo &info);
        Resource OutputDepthAttachment(string_view name, RenderTargetDesc desc, const AttachmentInfo &info);

        // The attachment at this index will become the LastOutput of the graph after the pass, defaults to 0
        void SetPrimaryAttachment(uint32 index);

        Resource CreateRenderTarget(string_view name, const RenderTargetDesc &desc);

        Resource CreateBuffer(BufferType bufferType, size_t size);
        Resource CreateBuffer(BufferType bufferType, string_view name, size_t size);

        // Defines a uniform buffer that will be shared between passes.
        Resource CreateUniformBuffer(string_view name, size_t size) {
            return CreateBuffer(BUFFER_TYPE_UNIFORM, name, size);
        }

        const Resource &ReadBuffer(string_view name);
        const Resource &ReadBuffer(ResourceID id);

        ResourceID LastOutputID() const {
            return resources.lastOutputID;
        }
        Resource LastOutput() const {
            return resources.LastOutput();
        }

        void RequirePass() {
            pass.required = true;
        }

    private:
        Resource OutputAttachment(uint32 index,
            string_view name,
            const RenderTargetDesc &desc,
            const AttachmentInfo &info);

        void SetAttachment(uint32 index, ResourceID id, const AttachmentInfo &info);
        void SetAttachmentWithoutOutput(uint32 index, ResourceID id, const AttachmentInfo &info);

        Resources &resources;
        Pass &pass;
    };
} // namespace sp::vulkan::render_graph
