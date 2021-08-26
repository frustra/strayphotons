#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"
#include "core/Logging.hh"

#include <atomic>
#include <memory>
#include <robin_hood.h>
#include <shared_mutex>
#include <string>
#include <vector>

namespace sp {
    static_assert(sizeof(chrono_clock::rep) <= sizeof(uint64_t), "Chrono Clock time point is larger than uint64_t");
    static_assert(ATOMIC_LLONG_LOCK_FREE == 2, "std::atomic_int64_t is not lock-free");

    template<typename T, int64_t PreserveAgeMilliseconds = 10000>
    class PreservingMap : public NonCopyable {
    private:
        static_assert(PreserveAgeMilliseconds > 0, "PreserveAgeMilliseconds must be positive");

        struct TimedValue {
            TimedValue() {}
            TimedValue(const std::shared_ptr<T> &value)
                : value(value), last_use(chrono_clock::now().time_since_epoch().count()) {}

            std::shared_ptr<T> value;
            std::atomic_int64_t last_use;
        };

        LockFreeMutex mutex;
        robin_hood::unordered_node_map<std::string, TimedValue> storage;

    public:
        PreservingMap() {}

        void Tick() {
            std::vector<std::string> cleanupList;
            {
                std::shared_lock lock(mutex);
                for (auto &[name, timed] : storage) {
                    if (timed.value.use_count() == 1) {
                        auto age = chrono_clock::now() -
                                   chrono_clock::time_point(std::chrono::steady_clock::duration(timed.last_use.load()));
                        auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(age).count();

                        if (ageMs > PreserveAgeMilliseconds) cleanupList.emplace_back(name);
                    } else {
                        timed.last_use = chrono_clock::now().time_since_epoch().count();
                    }
                }
            }
            if (cleanupList.size() > 0) {
                std::unique_lock lock(mutex);

                for (auto &name : cleanupList) {
                    auto it = storage.find(name);
                    if (it != storage.end() && it->second.value.use_count() == 1) {
                        auto age = chrono_clock::now() - chrono_clock::time_point(std::chrono::steady_clock::duration(
                                                             it->second.last_use.load()));
                        auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(age).count();
                        Debugf("PreservingMap: Removing %s, age: %d ms", name, ageMs);
                        storage.erase(it);
                    }
                }
            }
        }

        void Register(const std::string &name, const std::shared_ptr<T> &source) {
            std::unique_lock lock(mutex);

            Debugf("PreservingMap: Registering %s", name);
            bool ok = storage.emplace(name, source).second;
            Assert(ok, "Tried to register existing value in PreservingMap");
        }

        std::shared_ptr<T> Load(const std::string &name) {
            std::shared_lock lock(mutex);

            auto it = storage.find(name);
            if (it != storage.end()) {
                it->second.last_use = chrono_clock::now().time_since_epoch().count();
                return it->second.value;
            } else {
                return nullptr;
            }
        }
    };
} // namespace sp
