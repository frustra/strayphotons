#include "UniqueID.hh"

#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    HasUniqueID::HasUniqueID(DeviceContext &device) : uniqueID(device.NextUniqueID()) {}
} // namespace sp::vulkan
