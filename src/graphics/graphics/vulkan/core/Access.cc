#include "Access.hh"

namespace sp::vulkan {
    const AccessInfo AccessInfo::Map[(size_t)Access::AccessTypesCount] = {
        // Undefined
        {{}, {}, {}, {}, vk::ImageLayout::eUndefined},
        // IndirectBuffer
        {vk::PipelineStageFlagBits::eDrawIndirect,
            vk::AccessFlagBits::eIndirectCommandRead,
            vk::BufferUsageFlagBits::eIndirectBuffer,
            {},
            vk::ImageLayout::eUndefined},
        // IndexBuffer
        {vk::PipelineStageFlagBits::eVertexInput,
            vk::AccessFlagBits::eIndexRead,
            vk::BufferUsageFlagBits::eIndexBuffer,
            {},
            vk::ImageLayout::eUndefined},
        // VertexBuffer
        {vk::PipelineStageFlagBits::eVertexInput,
            vk::AccessFlagBits::eVertexAttributeRead,
            vk::BufferUsageFlagBits::eVertexBuffer,
            {},
            vk::ImageLayout::eUndefined},
        // VertexShaderSampleImage
        {vk::PipelineStageFlagBits::eVertexShader,
            vk::AccessFlagBits::eShaderRead,
            {},
            vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal},
        // VertexShaderReadUniform
        {vk::PipelineStageFlagBits::eVertexShader,
            vk::AccessFlagBits::eUniformRead,
            vk::BufferUsageFlagBits::eUniformBuffer,
            {},
            vk::ImageLayout::eUndefined},
        // VertexShaderReadStorage
        {vk::PipelineStageFlagBits::eVertexShader,
            vk::AccessFlagBits::eShaderRead,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::ImageUsageFlagBits::eStorage,
            vk::ImageLayout::eGeneral},
        // FragmentShaderSampleImage
        {vk::PipelineStageFlagBits::eFragmentShader,
            vk::AccessFlagBits::eShaderRead,
            {},
            vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal},
        // FragmentShaderReadUniform,
        {vk::PipelineStageFlagBits::eFragmentShader,
            vk::AccessFlagBits::eUniformRead,
            vk::BufferUsageFlagBits::eUniformBuffer,
            {},
            vk::ImageLayout::eUndefined},
        // FragmentShaderReadStorage
        {vk::PipelineStageFlagBits::eFragmentShader,
            vk::AccessFlagBits::eShaderRead,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::ImageUsageFlagBits::eStorage,
            vk::ImageLayout::eGeneral},
        // FragmentShaderReadColorInputAttachment
        {vk::PipelineStageFlagBits::eFragmentShader,
            vk::AccessFlagBits::eInputAttachmentRead,
            {},
            vk::ImageUsageFlagBits::eInputAttachment,
            vk::ImageLayout::eShaderReadOnlyOptimal},
        // FragmentShaderReadDepthStencilInputAttachment
        {vk::PipelineStageFlagBits::eFragmentShader,
            vk::AccessFlagBits::eInputAttachmentRead,
            {},
            vk::ImageUsageFlagBits::eInputAttachment,
            vk::ImageLayout::eDepthStencilReadOnlyOptimal},
        // ColorAttachmentRead
        {vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentRead,
            {},
            vk::ImageUsageFlagBits::eColorAttachment,
            vk::ImageLayout::eColorAttachmentOptimal},
        // DepthStencilAttachmentRead
        {vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::AccessFlagBits::eDepthStencilAttachmentRead,
            {},
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageLayout::eDepthStencilReadOnlyOptimal},
        // ComputeShaderSampleImage
        {vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderRead,
            {},
            vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal},
        // ComputeShaderReadUniform
        {vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eUniformRead,
            vk::BufferUsageFlagBits::eUniformBuffer,
            {},
            vk::ImageLayout::eUndefined},
        // ComputeShaderReadStorage
        {vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderRead,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::ImageUsageFlagBits::eStorage,
            vk::ImageLayout::eGeneral},
        // AnyShaderSampleImage
        {vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eShaderRead,
            {},
            vk::ImageUsageFlagBits::eSampled,
            vk::ImageLayout::eShaderReadOnlyOptimal},
        // AnyShaderReadUniform
        {vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eUniformRead,
            vk::BufferUsageFlagBits::eUniformBuffer,
            {},
            vk::ImageLayout::eUndefined},
        // AnyShaderReadStorage
        {vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eShaderRead,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::ImageUsageFlagBits::eStorage,
            vk::ImageLayout::eGeneral},
        // TransferRead
        {vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferRead,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::ImageUsageFlagBits::eTransferSrc,
            vk::ImageLayout::eTransferSrcOptimal},
        // HostRead
        {vk::PipelineStageFlagBits::eHost, vk::AccessFlagBits::eHostRead, {}, {}, vk::ImageLayout::eGeneral},
        // EndOfReads, not a valid index
        {{}, {}, {}, {}, vk::ImageLayout::eUndefined},
        // VertexShaderWrite
        {vk::PipelineStageFlagBits::eVertexShader,
            vk::AccessFlagBits::eShaderWrite,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::ImageUsageFlagBits::eStorage,
            vk::ImageLayout::eGeneral},
        // FragmentShaderWrite
        {vk::PipelineStageFlagBits::eFragmentShader,
            vk::AccessFlagBits::eShaderWrite,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::ImageUsageFlagBits::eStorage,
            vk::ImageLayout::eGeneral},
        // ColorAttachmentWrite,
        {vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentWrite,
            {},
            vk::ImageUsageFlagBits::eColorAttachment,
            vk::ImageLayout::eColorAttachmentOptimal},
        // ColorAttachmentReadWrite
        {vk::PipelineStageFlagBits::eColorAttachmentOutput,
            vk::AccessFlagBits::eColorAttachmentWrite,
            {},
            vk::ImageUsageFlagBits::eColorAttachment,
            vk::ImageLayout::eColorAttachmentOptimal},
        // DepthStencilAttachmentWrite
        {vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
            vk::AccessFlagBits::eDepthStencilAttachmentWrite,
            {},
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            vk::ImageLayout::eDepthStencilAttachmentOptimal},
        // ComputeShaderWrite
        {vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderWrite,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::ImageUsageFlagBits::eStorage,
            vk::ImageLayout::eGeneral},
        // AnyShaderWrite
        {vk::PipelineStageFlagBits::eAllCommands,
            vk::AccessFlagBits::eShaderWrite,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::ImageUsageFlagBits::eStorage,
            vk::ImageLayout::eGeneral},
        // TransferWrite
        {vk::PipelineStageFlagBits::eTransfer,
            vk::AccessFlagBits::eTransferWrite,
            vk::BufferUsageFlagBits::eTransferDst,
            vk::ImageUsageFlagBits::eTransferDst,
            vk::ImageLayout::eTransferDstOptimal},
        // HostPreinitialized
        {vk::PipelineStageFlagBits::eHost, vk::AccessFlagBits::eHostWrite, {}, {}, vk::ImageLayout::ePreinitialized},
        // HostWrite
        {vk::PipelineStageFlagBits::eHost, vk::AccessFlagBits::eHostWrite, {}, {}, vk::ImageLayout::eGeneral},
    };
} // namespace sp::vulkan
