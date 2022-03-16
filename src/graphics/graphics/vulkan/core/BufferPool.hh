#pragma once

#include "graphics/vulkan/core/Common.hh"
#include "graphics/vulkan/core/Memory.hh"

#include <robin_hood.h>

namespace sp::vulkan {
    class BufferPool {
    public:
        BufferPool(DeviceContext &device) : device(device) {}
        BufferPtr Get(const BufferDesc &desc);
        void Tick();
        void LogStats() const;

    private:
        DeviceContext &device;

        struct BufferList {
            vector<BufferPtr> free;
            vector<BufferPtr> pendingFree;
            vector<BufferPtr> pending;
        };
        robin_hood::unordered_map<BufferDesc, BufferList> buffers;
    };
} // namespace sp::vulkan
