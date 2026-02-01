/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "DispatchQueue.hh"

namespace sp {
    DispatchQueue::~DispatchQueue() {
        dropPendingWork = true;
        Shutdown();
    }

    void DispatchQueue::Shutdown() {
        ZoneScoped;
        std::unique_lock<std::mutex> lock(mutex);
        exit = true;
        workReady.notify_all();
        lock.unlock();

        for (auto &thread : threads) {
            if (thread.joinable()) thread.join();
        }
    }

    void DispatchQueue::Flush(bool blockUntilReady) {
        ZoneScoped;
        std::unique_lock<std::mutex> lock(mutex);
        FlushInternal(lock, workQueue.size(), blockUntilReady);
    }

    void DispatchQueue::ThreadMain() {
        tracy::SetThreadName(name.c_str());
        std::unique_lock<std::mutex> lock(mutex);

        while (true) {
            if (workQueue.empty()) {
                if (exit) break;
                workReady.wait(lock);
            }
            if (exit && dropPendingWork) break;
            if (workQueue.empty()) continue;

            {
                ZoneScopedN("ThreadFlush");
                size_t flushCount = workQueue.size();
                ZoneValue(flushCount);
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
        size_t flushCount = 0;
        while (maxWorkItems > 0 && !workQueue.empty()) {
            auto item = std::move(workQueue.front());
            workQueue.pop();

            lock.unlock();
            bool ready = blockUntilReady || item->Ready();
            if (ready) {
                ZoneScopedN("DispatchQueue::Process");
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
