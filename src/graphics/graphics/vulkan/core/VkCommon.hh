/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "graphics/vulkan/core/UniqueID.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vulkan/vulkan.hpp>

namespace sp::vulkan {
    typedef uint32 ShaderHandle;

    class DeviceContext;
    class CommandContext;
    class Buffer;
    class SubBuffer;
    class Image;
    class ImageView;
    typedef shared_ptr<CommandContext> CommandContextPtr;
    typedef shared_ptr<Buffer> BufferPtr;
    typedef shared_ptr<SubBuffer> SubBufferPtr;
    typedef shared_ptr<Image> ImagePtr;
    typedef shared_ptr<ImageView> ImageViewPtr;

    void AssertVKSuccess(vk::Result result, std::string message);
    void AssertVKSuccess(VkResult result, std::string message);

    enum QueueType { QUEUE_TYPE_GRAPHICS, QUEUE_TYPE_COMPUTE, QUEUE_TYPE_TRANSFER, QUEUE_TYPES_COUNT };

    enum class CommandContextType {
        General = QUEUE_TYPE_GRAPHICS,
        ComputeAsync = QUEUE_TYPE_COMPUTE,
        TransferAsync = QUEUE_TYPE_TRANSFER,
    };

    enum class CommandContextScope {
        Frame,
        Fence,
    };

    enum class SamplerType {
        BilinearClampEdge,
        BilinearClampBorder,
        BilinearTiled,
        TrilinearClampEdge,
        TrilinearClampBorder,
        TrilinearTiled,
        NearestClampEdge,
        NearestClampBorder,
        NearestTiled,
    };

    template<typename T>
    class WrappedUniqueHandle : public NonCopyable {
    public:
        T Get() const {
            return *uniqueHandle;
        }

        T operator*() const {
            return *uniqueHandle;
        }

        T *operator->() {
            T &t = *uniqueHandle;
            return &t;
        }

        operator T() const {
            return *uniqueHandle;
        }

    protected:
        vk::UniqueHandle<T, vk::DispatchLoaderDynamic> uniqueHandle;
    };
} // namespace sp::vulkan
