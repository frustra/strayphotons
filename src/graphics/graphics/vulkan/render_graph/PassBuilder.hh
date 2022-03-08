#pragma once

#include "graphics/vulkan/render_graph/Pass.hh"
#include "graphics/vulkan/render_graph/Resources.hh"

namespace sp::vulkan::render_graph {
    using PipelineStageMask = vk::PipelineStageFlags;
    using PipelineStage = vk::PipelineStageFlagBits;
    using AccessMask = vk::AccessFlags;
    using Access = vk::AccessFlagBits;
    using BufferUsageMask = vk::BufferUsageFlags;
    using BufferUsage = vk::BufferUsageFlagBits;

    class PassBuilder {
    public:
        PassBuilder(Resources &resources, Pass &pass, int framesAgo = 0)
            : resources(resources), pass(pass), framesAgo(framesAgo) {}

        ResourceID GetID(string_view name) {
            bool assertExists = (framesAgo == 0);
            return resources.GetID(name, assertExists, framesAgo);
        }

        Resource GetResource(ResourceID id) {
            return resources.GetResource(id);
        }
        Resource GetResource(string_view name) {
            return resources.GetResource(GetID(name));
        }

        const Resource &TransferRead(string_view name);
        const Resource &TransferRead(ResourceID id);
        const Resource &TransferWrite(string_view name);
        const Resource &TransferWrite(ResourceID id);

        // Images may be accessed as textures
        const Resource &TextureRead(string_view name, PipelineStageMask stages = PipelineStage::eFragmentShader);
        const Resource &TextureRead(ResourceID id, PipelineStageMask stages = PipelineStage::eFragmentShader);

        // Buffers may be read as uniforms
        const Resource &UniformRead(string_view name, PipelineStageMask stages = PipelineStage::eVertexShader);
        const Resource &UniformRead(ResourceID id, PipelineStageMask stages = PipelineStage::eVertexShader);

        // Buffers and images may be accessed as storage
        const Resource &StorageRead(string_view name, PipelineStageMask stages = PipelineStage::eFragmentShader);
        const Resource &StorageRead(ResourceID id, PipelineStageMask stages = PipelineStage::eFragmentShader);
        const Resource &StorageWrite(string_view name, PipelineStageMask stages = PipelineStage::eFragmentShader);
        const Resource &StorageWrite(ResourceID id, PipelineStageMask stages = PipelineStage::eFragmentShader);

        // Generic access, when one of the helpers is insufficient
        const Resource &BufferAccess(string_view name,
            PipelineStageMask stages,
            AccessMask access,
            BufferUsageMask usage);
        const Resource &BufferAccess(ResourceID id, PipelineStageMask stages, AccessMask access, BufferUsageMask usage);

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

        // If a pass has any attachments, it will automatically be treated as a render pass.
        // If a pass invokes the rasterizer but doesn't have any attachments, call MakeRenderPass.
        void MakeRenderPass() {
            pass.isRenderPass = true;
        }

        Resource RenderTargetCreate(string_view name, const RenderTargetDesc &desc);

        Resource BufferCreate(size_t size, BufferUsageMask usage, Residency residency);
        Resource BufferCreate(string_view name, size_t size, BufferUsageMask usage, Residency residency);

        Resource StorageCreate(size_t size, Residency residency) {
            return BufferCreate(size, BufferUsage::eStorageBuffer, residency);
        }
        Resource StorageCreate(string_view name, size_t size, Residency residency) {
            return BufferCreate(name, size, BufferUsage::eStorageBuffer, residency);
        }

        // Defines a uniform buffer that will be shared between passes.
        Resource UniformCreate(string_view name, size_t size) {
            return BufferCreate(name, size, BufferUsage::eUniformBuffer, Residency::CPU_TO_GPU);
        }

        ResourceID LastOutputID() const {
            return resources.lastOutputID;
        }
        Resource LastOutput() const {
            return resources.LastOutput();
        }

        void RequirePass() {
            pass.required = true;
        }

        PassBuilder PreviousFrame() {
            return {resources, pass, framesAgo + 1};
        }

        /**
         * Keeps a resource alive until the next frame's graph has been built.
         * If no pass depends on the resource in the next frame, it will be released before execution.
         */
        void ExportToNextFrame(ResourceID id) {
            resources.ExportToNextFrame(id);
        }
        void ExportToNextFrame(string_view name) {
            resources.ExportToNextFrame(name);
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
        int framesAgo;
    };
} // namespace sp::vulkan::render_graph
