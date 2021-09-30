#pragma once

#include "graphics/vulkan/core/Common.hh"

namespace sp::vulkan {
    class DeviceContext;

    void WriteScreenshot(DeviceContext &device, const std::string &path, const ImageViewPtr &view);
} // namespace sp::vulkan
