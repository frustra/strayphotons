#include "UniqueID.hh"

#include "DeviceContext.hh"

namespace sp::vulkan {
    HasUniqueID::HasUniqueID(DeviceContext &device) : uniqueID(device.NextUniqueID()) {}
} // namespace sp::vulkan
