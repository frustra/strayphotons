/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "graphics/vulkan/core/VkCommon.hh"

namespace sp::vulkan {
    enum class Access : uint8_t {
        None,

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
        SwapchainPresent,

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
