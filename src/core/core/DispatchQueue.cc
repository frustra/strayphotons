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
                    std::this_thread::sleep_for(flushSleepInterval);
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
