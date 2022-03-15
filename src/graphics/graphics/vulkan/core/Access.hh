#pragma once

#include "graphics/vulkan/core/Common.hh"

namespace sp::vulkan {
    enum class Access : uint8_t {
        Undefined,

        // Reads
        IndirectBuffer,
        IndexBuffer,
        VertexBuffer,
        VertexShaderSampleImage,
        VertexShaderReadUniform,
        VertexShaderReadStorage,
        FragmentShaderSampleImage,
        FragmentShaderReadUniform,
        FragmentShaderReadStorage,
        FragmentShaderReadColorInputAttachment,
        FragmentShaderReadDepthInputStencilAttachment,
        ColorAttachmentRead,
        DepthStencilAttachmentRead,
        ComputeShaderSampleImage,
        ComputeShaderReadUniform,
        ComputeShaderReadStorage,
        AnyShaderSampleImage,
        AnyShaderReadUniform,
        AnyShaderReadStorage,
        TransferRead,
        HostRead,

        AccessTypesEndOfReads,

        // Writes
        VertexShaderWrite,
        FragmentShaderWrite,
        ColorAttachmentWrite,
        ColorAttachmentReadWrite,
        DepthStencilAttachmentWrite,
        ComputeShaderWrite,
        AnyShaderWrite,
        TransferWrite,
        HostPreinitialized,
        HostWrite,

        AccessTypesCount,
    };

    inline bool AccessIsWrite(Access access) {
        return access > Access::AccessTypesEndOfReads && access < Access::AccessTypesCount;
    }

    struct AccessInfo {
        vk::PipelineStageFlags stageMask;
        vk::AccessFlags accessMask;
        vk::BufferUsageFlags bufferUsageMask;
        vk::ImageUsageFlags imageUsageMask;
        vk::ImageLayout imageLayout;

        static const AccessInfo Map[(size_t)Access::AccessTypesCount];
    };

    inline const AccessInfo &GetAccessInfo(Access type) {
        return AccessInfo::Map[(size_t)type];
    }
} // namespace sp::vulkan
