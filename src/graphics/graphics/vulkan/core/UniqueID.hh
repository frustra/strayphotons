#pragma once

#include "core/Common.hh"

#include <cstdint>

namespace sp::vulkan {
    class DeviceContext;

    typedef uint64_t UniqueID;

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
