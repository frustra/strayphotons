/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "graphics/vulkan/core/UniqueID.hh"
#include "strayphotons/Utility.hh"
#include "vulkan/vulkan.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <string>
#include <vulkan/vulkan.hpp>

namespace sp::vulkan {
    typedef uint32_t ShaderHandle;

    class DeviceContext;
    class CommandContext;
    class Buffer;
    class SubBuffer;
    class Image;
    class ImageView;
    typedef std::shared_ptr<CommandContext> CommandContextPtr;
    typedef std::shared_ptr<Buffer> BufferPtr;
    typedef std::shared_ptr<SubBuffer> SubBufferPtr;
    typedef std::shared_ptr<Image> ImagePtr;
    typedef std::shared_ptr<ImageView> ImageViewPtr;
    typedef std::shared_ptr<vk::UniqueSemaphore> SemaphorePtr;

    void AssertVKSuccess(vk::Result result, std::string message);
    void AssertVKSuccess(VkResult result, std::string message);

    enum class QueueID {
        General = 0,
        ComputeAsync,
        TransferAsync,
    };

    enum class CommandContextType {
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
        vk::UniqueHandle<T, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE> uniqueHandle;
    };
} // namespace sp::vulkan
