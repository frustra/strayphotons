#include "BufferPool.hh"

#include "console/CVar.hh"
#include "graphics/vulkan/core/DeviceContext.hh"

namespace sp::vulkan {

    BufferPtr BufferPool::Get(const BufferDesc &desc) {
        auto &bufferList = buffers[desc];

        if (!bufferList.free.empty()) {
            auto entry = std::move(bufferList.free.back());
            bufferList.free.pop_back();

            entry.unusedFrames = 0;
            bufferList.pending.push_back(entry);
            return entry.ptr;
        }

        bufferList.pending.emplace_back(BufferEntry{device.AllocateBuffer(desc.layout, desc.usage, (VmaMemoryUsage)desc.residency),
            0});
        return bufferList.pending.back().ptr;
    }

    void BufferPool::Tick() {
        ZoneScoped;
        for (auto &[desc, list] : buffers) {
            erase_if(list.free, [&](auto &entry) {
                return entry.unusedFrames++ > 4;
            });

            list.free.insert(list.free.end(), list.pendingFree.begin(), list.pendingFree.end());
            list.pendingFree.clear();

            for (int i = list.pending.size() - 1; i >= 0; i--) {
                auto &entry = list.pending[i];
                if (entry.ptr.use_count() == 1) {
                    // Host visible memory may be mapped and written while GPU work is still ongoing.
                    // Double buffer in this case, so the next frame can start without synchronizing.
                    if (entry.ptr->Properties() & vk::MemoryPropertyFlagBits::eHostVisible)
                        list.pendingFree.push_back(std::move(entry));
                    else
                        list.free.push_back(std::move(entry));
                    list.pending.erase(list.pending.begin() + i);
                }
            }
        }
    }

    void BufferPool::LogStats() const {
        bool any = false;
        struct entry {
            struct {
                size_t count = 0, bytes = 0;
            } allocated, free, pendingFree;
        };
        std::unordered_map<VkMemoryPropertyFlags, entry> stats;

        for (auto &[desc, list] : buffers) {
            for (auto &entry : list.pending) {
                auto &buf = entry.ptr;
                auto &ent = stats[VkMemoryPropertyFlags(buf->Properties())];
                ent.allocated.bytes += buf->ByteSize();
                ent.allocated.count += 1;
                any = true;
            }
            for (auto &entry : list.free) {
                auto &buf = entry.ptr;
                auto &ent = stats[VkMemoryPropertyFlags(buf->Properties())];
                ent.free.bytes += buf->ByteSize();
                ent.free.count += 1;
                any = true;
            }
            for (auto &entry : list.pendingFree) {
                auto &buf = entry.ptr;
                auto &ent = stats[VkMemoryPropertyFlags(buf->Properties())];
                ent.pendingFree.bytes += buf->ByteSize();
                ent.pendingFree.count += 1;
                any = true;
            }
        }

        if (!any) return;

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
