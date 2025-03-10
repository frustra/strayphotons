/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/Common.hh"
#include "common/InlineVector.hh"
#include "common/LockFreeMutex.hh"
#include "common/Logging.hh"

#include <deque>
#include <mutex>
#include <robin_hood.h>
#include <set>
#include <shared_mutex>
#include <string>
#include <unordered_set>

namespace sp {
    static_assert(sizeof(chrono_clock::rep) <= sizeof(uint64_t), "Chrono Clock time point is larger than uint64_t");
    static_assert(ATOMIC_LLONG_LOCK_FREE == 2, "std::atomic_int64_t is not lock-free");

    template<typename T,
        int64_t PreserveAgeMilliseconds = 10000,
        typename Hash = robin_hood::hash<T>,
        typename Equal = std::equal_to<T>>
    class PreservingSet : public NonCopyable {
    private:
        static_assert(PreserveAgeMilliseconds > 0, "PreserveAgeMilliseconds must be positive");

        struct TimedValue {
            TimedValue() {}
            TimedValue(const T &value) : value(value), last_use(0) {}

            TimedValue &operator=(TimedValue &&other) {
                value = std::move(other.value);
                ptr = std::move(other.ptr);
                last_use = 0;
                return *this;
            }

            std::optional<T> value;
            std::weak_ptr<T> ptr;
            std::atomic_uint64_t last_use;
        };

        struct PtrHash {
            using is_transparent = void;

            size_t operator()(const std::shared_ptr<T> &ptr) const {
                if (!ptr) return Hash{}(ptr);
                return Hash{}(*ptr);
            }

            size_t operator()(const T &value) const {
                return Hash{}(value);
            }
        };

        struct PtrEqual {
            using is_transparent = void;

            bool operator()(const std::shared_ptr<T> &a, const std::shared_ptr<T> &b) const {
                return a == b;
            }

            bool operator()(const std::shared_ptr<T> &a, const T &b) const {
                if (!a) return false;
                return Equal{}(*a, b);
            }

            bool operator()(const T &a, const std::shared_ptr<T> &b) const {
                if (!b) return false;
                return Equal{}(a, *b);
            }
        };

        LockFreeMutex mutex;
        chrono_clock::time_point last_tick;
        std::deque<TimedValue> storage;
        std::unordered_set<std::shared_ptr<T>, PtrHash, PtrEqual> handles;

        robin_hood::unordered_flat_map<T *, size_t> indexLookup;
        std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>> freeList;

    public:
        PreservingSet() {}

        void Tick(chrono_clock::duration maxTickInterval) {
            auto now = chrono_clock::now();
            chrono_clock::duration tickInterval = std::min(now - last_tick, maxTickInterval);
            auto intervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(tickInterval).count();
            last_tick = now;

            InlineVector<size_t, 100> cleanupList;
            {
                std::shared_lock lock(mutex);
                for (size_t i = 0; i < storage.size(); i++) {
                    auto &timed = storage[i];
                    if (timed.ptr.use_count() == 0) continue; // Weak ptr empty
                    if (timed.ptr.use_count() == 1) {
                        if ((timed.last_use += intervalMs) > PreserveAgeMilliseconds) {
                            if (cleanupList.size() < cleanupList.capacity()) cleanupList.push_back(i);
                        }
                    } else {
                        timed.last_use = 0;
                    }
                }
            }
            if (cleanupList.size() > 0) {
                std::unique_lock lock(mutex);

                for (size_t &i : cleanupList) {
                    if (i < storage.size()) {
                        auto &timed = storage[i];
                        if (timed.ptr.use_count() == 1) {
                            std::shared_ptr<T> handle = timed.ptr.lock();
                            handles.erase(handle);
                            indexLookup.erase(handle.get());
                            handle.reset();
                            timed.value.reset();
                            Assertf(timed.ptr.use_count() == 0, "PreservingSet handle delete failed");
                            freeList.push(i);
                        }
                    }
                }
            }
        }

        std::shared_ptr<T> LoadOrInsert(const T &value) {
            std::unique_lock lock(mutex);

            auto it = handles.find(value);
            if (it != handles.end()) {
                std::shared_ptr<T> ptr = *it;
                size_t i = indexLookup.at(ptr.get());
                Assertf(i < storage.size(), "PreservingSet index out of bounds");
                storage[i].last_use = 0;
                return ptr;
            } else {
                size_t i = ~0u;
                if (!freeList.empty()) {
                    i = freeList.top();
                    freeList.pop();
                    storage[i] = TimedValue(value);
                } else {
                    i = storage.size();
                    storage.emplace_back(value);
                }
                Assertf(i < storage.size(), "PreservingSet index out of bounds");
                std::shared_ptr<T> ptr(&storage[i].value.value(), [](auto *) {});
                storage[i].ptr = std::weak_ptr<T>(ptr);
                indexLookup.emplace(ptr.get(), i);
                handles.emplace(ptr);
                return ptr;
            }
        }

        std::shared_ptr<T> Find(const T &value) {
            std::shared_lock lock(mutex);

            auto it = handles.find(value);
            if (it != handles.end()) {
                std::shared_ptr<T> ptr = *it;
                size_t i = indexLookup.at(ptr.get());
                Assertf(i < storage.size(), "PreservingSet index out of bounds");
                storage[i].last_use = 0;
                return ptr;
            } else {
                return nullptr;
            }
        }

        // Removes all values that have no references.
        // Values will have their destructors called inline by the current thread.
        // Returns the number of values that were removed.
        size_t DropAll(std::function<void(std::shared_ptr<T> &)> destroyCallback = nullptr) {
            std::unique_lock lock(mutex);

            size_t count = 0;
            for (size_t i = 0; i < storage.size(); i++) {
                // Nodes may reference earlier indexes, so drop in reverse order.
                auto &timed = storage[storage.size() - i - 1];
                if (timed.ptr.use_count() == 1) {
                    std::shared_ptr<T> handle = timed.ptr.lock();
                    if (destroyCallback) destroyCallback(handle);
                    handles.erase(handle);
                    indexLookup.erase(handle.get());
                    handle.reset();
                    timed.value.reset();
                    Assertf(timed.ptr.use_count() == 0, "PreservingSet handle delete failed");
                    freeList.emplace(storage.size() - i - 1);
                    count++;
                }
            }
            return count;
        }

        void ForEach(std::function<void(T &, std::shared_ptr<T>)> callback) {
            std::unique_lock lock(mutex);
            for (auto &timed : storage) {
                if (timed.value) callback(*timed.value, timed.ptr.lock());
            }
        }

        bool Contains(const T &value) {
            std::shared_lock lock(mutex);
            return storage.contains(value);
        }

        size_t Size() {
            std::shared_lock lock(mutex);
            return storage.size() - freeList.size();
        }
    };
} // namespace sp
