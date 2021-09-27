#pragma once

#include "DeviceContext.hh"

namespace sp::vulkan {
    void WriteScreenshot(DeviceContext &device, const std::string &path, const ImageViewPtr &view);
}
