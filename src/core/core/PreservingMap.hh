#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "core/Logging.hh"

#include <atomic>
#include <memory>
#include <mutex>
#include <robin_hood.h>
#include <shared_mutex>
#include <string>
#include <vector>

namespace sp {
    static_assert(sizeof(chrono_clock::rep) <= sizeof(uint64_t), "Chrono Clock time point is larger than uint64_t");
    static_assert(ATOMIC_LLONG_LOCK_FREE == 2, "std::atomic_int64_t is not lock-free");

    template<typename K, typename V, int64_t PreserveAgeMilliseconds = 10000>
    class PreservingMap : public NonCopyable {
    private:
        static_assert(PreserveAgeMilliseconds > 0, "PreserveAgeMilliseconds must be positive");

        struct TimedValue {
            TimedValue() {}
            TimedValue(const std::shared_ptr<V> &value) : value(value), last_use(0) {}

            std::shared_ptr<V> value;
            std::atomic_uint64_t last_use;
        };

        LockFreeMutex mutex;
        robin_hood::unordered_node_map<K, TimedValue> storage;

    public:
        PreservingMap() {}

        void Tick(chrono_clock::duration tickInterval) {
            std::vector<K> cleanupList;
            {
                std::shared_lock lock(mutex);
                for (auto &[key, timed] : storage) {
                    if (timed.value.use_count() == 1) {
                        auto intervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(tickInterval).count();
                        if ((timed.last_use += intervalMs) > PreserveAgeMilliseconds) cleanupList.emplace_back(key);
                    } else {
                        timed.last_use = 0;
                    }
                }
            }
            if (cleanupList.size() > 0) {
                std::unique_lock lock(mutex);

                for (auto &key : cleanupList) {
                    auto it = storage.find(key);
                    if (it != storage.end() && it->second.value.use_count() == 1) storage.erase(it);
                }
            }
        }

        void Register(const K &key, const std::shared_ptr<V> &source, bool allowReplace = false) {
            std::unique_lock lock(mutex);

            auto [it, inserted] = storage.emplace(key, source);
            if (!inserted) {
                Assert(allowReplace, "Tried to register existing value in PreservingMap");
                it->second.last_use = 0;
                it->second.value = source;
            }
        }

        std::shared_ptr<V> Load(const K &key) {
            std::shared_lock lock(mutex);

            auto it = storage.find(key);
            if (it != storage.end()) {
                it->second.last_use = 0;
                return it->second.value;
            } else {
                return nullptr;
            }
        }

        // Returns true if the key was dropped, or if it does not exist
        // A key can only be dropped if there are no references to it.
        // Values will have their destructors called inline by the current thread.
        bool Drop(const K &key) {
            std::unique_lock lock(mutex);

            auto it = storage.find(key);
            if (it == storage.end()) return true;

            if (it->second.value.use_count() == 1) {
                storage.erase(it);
                return true;
            }
            return false;
        }

        // Removes all values that have no references.
        // Values will have their destructors called inline by the current thread.
        void DropAll() {
            std::unique_lock lock(mutex);

            for (auto it = storage.begin(); it != storage.end(); it++) {
                if (it->second.value.use_count() == 1) storage.erase(it);
            }
        }
    };
} // namespace sp
