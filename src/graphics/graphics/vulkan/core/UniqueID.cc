#include "UniqueID.hh"

#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    static UniqueID NextUniqueID() {
        static UniqueID lastUniqueID = 0;
        return ++lastUniqueID;
    }

    HasUniqueID::HasUniqueID() : uniqueID(NextUniqueID()) {}
} // namespace sp::vulkan
