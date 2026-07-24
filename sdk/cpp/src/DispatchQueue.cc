/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "strayphotons/DispatchQueue.hh"

#include "strayphotons/Utility.hh"

#include <sstream>

#ifdef TRACY_ENABLE
    #include "common/Tracing.hh"
#endif

namespace sp {

    HeapString DispatchSourceInfo::String() const {
        std::stringstream ss;
        ss << filename << ":" << lineNumber << " in function " << function;
        return ss.view();
    }

    DispatchQueue::~DispatchQueue() {
        dropPendingWork = true;
        Shutdown();
    }

    void DispatchQueue::Shutdown() {
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::unique_lock<std::mutex> lock(mutex);
        exit = true;
        workReady.notify_all();
        lock.unlock();

        for (auto &thread : threads) {
            if (thread.joinable()) thread.join();
        }
    }

    void DispatchQueue::Flush(bool blockUntilReady, chrono_clock::duration timeLimit) {
#ifdef TRACY_ENABLE
        ZoneScoped;
#endif
        std::unique_lock<std::mutex> lock(mutex);
        size_t maxWorkItems = workQueue.size();
        if (maxWorkItems == 0) return;
        if (blockUntilReady || timeLimit.count() == 0) {
            FlushInternal(lock, maxWorkItems, blockUntilReady);
        } else {
            chrono_clock::time_point start = chrono_clock::now();
            while (maxWorkItems > 0 && chrono_clock::now() - start < timeLimit) {
                FlushInternal(lock, 1, false);
                maxWorkItems--;
            }
        }
    }

    void DispatchQueue::ThreadMain() {
#ifdef TRACY_ENABLE
        tracy::SetThreadName(name.c_str());
#endif
        std::unique_lock<std::mutex> lock(mutex);

        while (true) {
            if (workQueue.empty()) {
                if (exit) break;
                workReady.wait(lock);
            }
            if (exit && dropPendingWork) break;
            if (workQueue.empty()) continue;

            {
#ifdef TRACY_ENABLE
                ZoneScopedN("ThreadFlush");
#endif
                size_t flushCount = workQueue.size();
#ifdef TRACY_ENABLE
                ZoneValue(flushCount);
#endif
                if (FlushInternal(lock, flushCount, false) == 0) {
                    lock.unlock();
                    if (flushSleepInterval.count() > 0) {
                        std::this_thread::sleep_for(flushSleepInterval);
                    } else {
                        std::this_thread::yield();
                    }
                    lock.lock();
                }
            }
        }
    }

    size_t DispatchQueue::FlushInternal(std::unique_lock<std::mutex> &lock, size_t maxWorkItems, bool blockUntilReady) {
#ifdef TRACY_ENABLE
        ZoneScoped;
        ZoneValue(maxWorkItems);
#endif
        size_t flushCount = 0;
        while (maxWorkItems > 0 && !workQueue.empty()) {
            auto item = std::move(workQueue.front());
            workQueue.pop();

            lock.unlock();
            bool ready = blockUntilReady || item->Ready();
            if (ready) {
#ifdef TRACY_ENABLE
                ZoneScopedN("DispatchQueue::Process");
                DebugZoneStr(item->sourceInfo.String());
#endif
                item->Process();
                std::this_thread::yield();
                flushCount++;
            }
            lock.lock();

            if (!ready) workQueue.push(std::move(item));
            maxWorkItems--;
        }
        return flushCount;
    }
} // namespace sp
