#pragma once

#include "core/Common.hh"

namespace sp::vulkan {
    class DeviceContext;

    typedef uint64 UniqueID;

    class HasUniqueID {
    public:
        HasUniqueID();

        UniqueID GetUniqueID() const {
            return uniqueID;
        }

    private:
        const UniqueID uniqueID;
    };
} // namespace sp::vulkan
