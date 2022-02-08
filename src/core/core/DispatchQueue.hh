#pragma once

#include "Common.hh"
#include "Tracing.hh"

#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace sp {
    struct DispatchQueueWorkItemBase {
        virtual void Process() = 0;
        virtual bool Ready() = 0;
    };
    template<typename WorkResult, typename Fn, typename... WaitForFutureTypes>
    struct DispatchQueueWorkItem : public DispatchQueueWorkItemBase {
        DispatchQueueWorkItem(Fn &&func, std::future<WaitForFutureTypes> &&...futures)
            : func(std::move(func)), waitForFutures(std::move(futures)...) {}

        std::promise<WorkResult> promise;
        std::function<WorkResult(WaitForFutureTypes...)> func;
        std::tuple<std::future<WaitForFutureTypes>...> waitForFutures;

        void Process() {
            ZoneScoped;
            std::tuple<WaitForFutureTypes...> values;
            {
                ZoneScopedN("WaitForFutures");
                values = std::apply(
                    [&](auto &&...x) {
                        return std::make_tuple((x).get()...);
                    },
                    waitForFutures);
            }
            if constexpr (std::is_same<WorkResult, void>()) {
                std::apply(func, values);
                promise.set_value();
            } else {
                promise.set_value(std::apply(func, values));
            }
        }

        bool Ready() {
            bool ready = true;
            std::apply(
                [&ready](auto &&...fut) {
                    ((ready &= (fut.wait_for(std::chrono::seconds(0)) != std::future_status::timeout)), ...);
                },
                waitForFutures);
            return ready;
        }
    };

    class DispatchQueue : public NonCopyable {
    public:
        DispatchQueue(std::string name,
            size_t threadCount = 1,
            chrono_clock::duration futuresPollInterval = std::chrono::milliseconds(5))
            : name(std::move(name)), threads(threadCount), flushSleepInterval(futuresPollInterval) {
            for (auto &thread : threads) {
                thread = std::thread(&DispatchQueue::ThreadMain, this);
            }
        }

        ~DispatchQueue();

        void Shutdown();

        template<typename WorkResult, typename Fn, typename... WaitForFutureTypes>
        std::future<WorkResult> Dispatch(Fn &&func, std::future<WaitForFutureTypes> &&...futures) {
            Assert(!exit, "tried to dispatch to a shut down queue");
            auto item = make_shared<DispatchQueueWorkItem<WorkResult, Fn, WaitForFutureTypes...>>(std::move(func),
                std::move(futures)...);

            std::unique_lock<std::mutex> lock(mutex);
            auto fut = item->promise.get_future();
            workQueue.push(item);
            workReady.notify_one();
            return fut;
        }

        void Flush(bool blockUntilReady = false);

    private:
        size_t FlushInternal(std::unique_lock<std::mutex> &lock, size_t maxWorkItems, bool blockUntilReady);
        void ThreadMain();

        std::mutex mutex;
        std::string name;
        std::vector<std::thread> threads;
        chrono_clock::duration flushSleepInterval;

        std::queue<shared_ptr<DispatchQueueWorkItemBase>> workQueue;
        std::condition_variable workReady;
        bool exit = false;
    };
} // namespace sp
