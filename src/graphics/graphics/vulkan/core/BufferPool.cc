#include "BufferPool.hh"

#include "console/CVar.hh"
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
            list.free.insert(list.free.end(), list.pendingFree.begin(), list.pendingFree.end());
            list.pendingFree.clear();

            for (int i = list.pending.size() - 1; i >= 0; i--) {
                auto &x = list.pending[i];
                if (x.use_count() == 1) {
                    if (x->Properties() & vk::MemoryPropertyFlagBits::eHostVisible)
                        list.pendingFree.push_back(std::move(x));
                    else
                        list.free.push_back(std::move(x));
                    list.pending.erase(list.pending.begin() + i);
                }
            }
        }
    }

    void BufferPool::LogStats() const {
        struct entry {
            struct {
                size_t count = 0, bytes = 0;
            } allocated, free, pendingFree;
        };
        std::unordered_map<VkMemoryPropertyFlags, entry> stats;

        for (auto &[desc, list] : buffers) {
            for (auto &buf : list.pending) {
                auto &ent = stats[VkMemoryPropertyFlags(buf->Properties())];
                ent.allocated.bytes += buf->ByteSize();
                ent.allocated.count += 1;
            }
            for (auto &buf : list.free) {
                auto &ent = stats[VkMemoryPropertyFlags(buf->Properties())];
                ent.free.bytes += buf->ByteSize();
                ent.free.count += 1;
            }
            for (auto &buf : list.pendingFree) {
                auto &ent = stats[VkMemoryPropertyFlags(buf->Properties())];
                ent.pendingFree.bytes += buf->ByteSize();
                ent.pendingFree.count += 1;
            }
        }

        for (auto &[key, ent] : stats) {
            Logf("%s\n%llu count allocated, %llu count free, %llu count pending free\n%llu bytes allocated, %llu bytes "
                 "free, %llu bytes pending free",
                vk::to_string(vk::MemoryPropertyFlags(key)),
                ent.allocated.count,
                ent.free.count,
                ent.pendingFree.count,
                ent.allocated.bytes,
                ent.free.bytes,
                ent.pendingFree.bytes);
        }
    }
} // namespace sp::vulkan
