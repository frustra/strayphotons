#include "StringHandle.hh"

#include <mutex>

namespace ecs {
    StringHandleManager &GetStringHandler() {
        static StringHandleManager entityRefs;
        return entityRefs;
    }

    StringHandle StringHandleManager::Get(const std::string &str) {
        StringHandle handle = strings.Load(str);
        if (!handle) {
            std::lock_guard lock(mutex);
            handle = strings.Load(str);
            if (handle) return handle;

            handle = std::make_shared<std::string>(str);
            strings.Register(str, handle);
        }
        return handle;
    }

    void StringHandleManager::Tick(chrono_clock::duration maxTickInterval) {
        strings.Tick(maxTickInterval);
    }
} // namespace ecs
