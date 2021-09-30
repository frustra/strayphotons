#pragma once

#include "graphics/vulkan/core/Common.hh"

namespace sp::vulkan {
    void WriteScreenshot(DeviceContext &device, const std::string &path, const ImageViewPtr &view);
}
