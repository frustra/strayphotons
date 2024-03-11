/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/InlineVector.hh"
#include "common/LockFreeMutex.hh"
#include "common/Logging.hh"

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

    template<typename K,
        typename V,
        int64_t PreserveAgeMilliseconds = 10000,
        typename Hash = robin_hood::hash<K>,
        typename Equal = std::equal_to<K>>
    class PreservingMap : public NonCopyable {
    private:
        static_assert(PreserveAgeMilliseconds > 0, "PreserveAgeMilliseconds must be positive");

        struct TimedValue {
            TimedValue() {}
            TimedValue(const std::shared_ptr<V> &value) : value(value), last_use(0) {}

            std::shared_ptr<V> value;
            std::atomic_uint64_t last_use;
        };

        using Storage = robin_hood::unordered_node_map<K, TimedValue, Hash, Equal>;

        LockFreeMutex mutex;
        chrono_clock::time_point last_tick;
        Storage storage;

    public:
        PreservingMap() : last_tick(chrono_clock::now()) {}

        void Tick(chrono_clock::duration maxTickInterval,
            std::function<void(std::shared_ptr<V> &)> destroyCallback = nullptr) {
            auto now = chrono_clock::now();
            chrono_clock::duration tickInterval = std::min(now - last_tick, maxTickInterval);
            last_tick = now;

            InlineVector<K, 100> cleanupList;
            {
                std::shared_lock lock(mutex);
                for (auto &[key, timed] : storage) {
                    if (timed.value.use_count() == 1) {
                        auto intervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(tickInterval).count();
                        if ((timed.last_use += intervalMs) > PreserveAgeMilliseconds) {
                            if (cleanupList.size() < cleanupList.capacity()) cleanupList.emplace_back(key);
                        }
                    } else {
                        timed.last_use = 0;
                    }
                }
            }
            if (cleanupList.size() > 0) {
                std::unique_lock lock(mutex);

                for (auto &key : cleanupList) {
                    auto it = storage.find(key);
                    if (it != storage.end() && it->second.value.use_count() == 1) {
                        if (destroyCallback) destroyCallback(it->second.value);
                        storage.erase(it);
                    }
                }
            }
        }

        void Register(const K &key, const std::shared_ptr<V> &source, bool allowReplace = false) {
            std::unique_lock lock(mutex);

            auto [it, inserted] = storage.emplace(key, source);
            if (!inserted) {
                Assertf(allowReplace, "Tried to register existing value in PreservingMap");
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

        template<typename OtherKey, typename S = Storage>
        typename std::enable_if<S::is_transparent, std::shared_ptr<V>>::type Load(const OtherKey &key) {
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
        // Returns the number of values that were removed.
        size_t DropAll(std::function<void(std::shared_ptr<V> &)> destroyCallback = nullptr) {
            std::unique_lock lock(mutex);

            size_t count = 0;
            for (auto it = storage.begin(); it != storage.end();) {
                if (it->second.value.use_count() == 1) {
                    if (destroyCallback) destroyCallback(it->second.value);
                    it = storage.erase(it);
                    count++;
                } else {
                    it++;
                }
            }
            return count;
        }

        void ForEach(std::function<void(const K &, std::shared_ptr<V> &)> callback) {
            std::unique_lock lock(mutex);
            for (auto &[key, tvalue] : storage) {
                callback(key, tvalue.value);
            }
        }

        bool Contains(const K &key) {
            std::shared_lock lock(mutex);
            return storage.contains(key);
        }

        template<typename OtherKey, typename S = Storage>
        typename std::enable_if<S::is_transparent, bool>::type Contains(const OtherKey &key) {
            std::shared_lock lock(mutex);
            return storage.contains(key);
        }
    };
} // namespace sp
