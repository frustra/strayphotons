#include "BufferPool.hh"

#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {
    BufferPtr BufferPool::Get(const BufferDesc &desc) {
        auto &bufferList = buffers[desc];

        if (!bufferList.free.empty()) {
            auto buffer = std::move(bufferList.free.back());
            bufferList.free.pop_back();
            bufferList.pending.push_back(buffer);
            return buffer;
        }

        bufferList.pending.push_back(device.AllocateBuffer(desc.size, desc.usage, (VmaMemoryUsage)desc.residency));
        return bufferList.pending.back();
    }

    void BufferPool::Tick() {
        for (auto &[desc, list] : buffers) {
            for (int i = list.pending.size() - 1; i >= 0; i--) {
                auto &x = list.pending[i];
                if (x.use_count() == 1) {
                    list.free.push_back(std::move(x));
                    list.pending.erase(list.pending.begin() + i);
                }
            }
        }
    }
} // namespace sp::vulkan
