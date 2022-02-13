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
    namespace detail {
        template<typename... T, std::size_t... I>
        auto subtuple(std::tuple<T...> &&t, std::index_sequence<I...>) {
            return std::make_tuple(std::move(std::get<I>(t))...);
        }

        template<auto Start, auto End, auto Increment = 1, class Fn>
        constexpr bool constexpr_for(Fn &&f) {
            if constexpr (Start < End) {
                auto continued = f.template operator()<Start>();
                if (continued) return constexpr_for<Start + Increment, End>(f);
                return false;
            } else {
                return true;
            }
        }
    } // namespace detail

    struct DispatchQueueWorkItemBase {
        virtual void Process() = 0;
        virtual bool Ready() = 0;
    };
    template<typename ReturnType, typename Fn, typename... ParameterTypes>
    struct DispatchQueueWorkItem : public DispatchQueueWorkItemBase {
        using FutureTuple = std::tuple<std::future<ParameterTypes>...>;

        DispatchQueueWorkItem(Fn &&func, FutureTuple &&futures)
            : func(std::move(func)), waitForFutures(std::move(futures)) {}

        std::promise<ReturnType> promise;
        std::function<ReturnType(ParameterTypes...)> func;
        FutureTuple waitForFutures;

        void Process() {
            ZoneScoped;
            std::tuple<ParameterTypes...> args;
            {
                ZoneScopedN("WaitForFutures");
                args = std::apply(
                    [](auto &&...x) {
                        return std::make_tuple(x.get()...);
                    },
                    waitForFutures);
            }
            if constexpr (std::is_same<ReturnType, void>()) {
                std::apply(func, args);
                promise.set_value();
            } else {
                promise.set_value(std::apply(func, args));
            }
        }

        bool Ready() {
            return detail::constexpr_for<0, sizeof...(ParameterTypes)>([this]<int I>() {
                return std::get<I>(waitForFutures).wait_for(std::chrono::seconds(0)) != std::future_status::timeout;
            });
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
        void Flush(bool blockUntilReady = false);

        /**
         * Queues a function.
         * Returns a future that will be resolved with the return value of the function.
         * After input futures are ready, the function will be called with their values.
         *
         * Usage: Dispatch<R>(std::future<T>..., [](T...) { return R(); });
         * Example:
         *  auto image = queue.Dispatch<ImagePtr>([]() { return MakeImagePtr(); });
         *  queue.Dispatch<void>(std::move(image), [](ImagePtr image) { });
         */
        template<typename ReturnType, typename... FuturesAndFn>
        std::future<ReturnType> Dispatch(FuturesAndFn &&...args) {
            // if C++ adds support for parameters after a parameter pack, delete this function
            const size_t lastArg = sizeof...(FuturesAndFn) - 1;
            auto tupl = std::make_tuple(std::move(args)...);
            auto fn = std::move(std::get<lastArg>(tupl));
            auto futures = detail::subtuple(std::move(tupl), std::make_index_sequence<lastArg>());
            return DispatchInternal<ReturnType>(std::move(futures), std::move(fn));
        }

    private:
        template<typename ReturnType, typename Fn, typename... ParameterTypes>
        std::future<ReturnType> DispatchInternal(std::tuple<std::future<ParameterTypes>...> &&futures, Fn func) {
            Assert(!exit, "tried to dispatch to a shut down queue");
            auto item = make_shared<DispatchQueueWorkItem<ReturnType, Fn, ParameterTypes...>>(std::move(func),
                std::move(futures));

            std::unique_lock<std::mutex> lock(mutex);
            auto fut = item->promise.get_future();
            workQueue.push(item);
            workReady.notify_one();
            return fut;
        }

        size_t FlushInternal(std::unique_lock<std::mutex> &lock, size_t maxWorkItems, bool blockUntilReady);
        void ThreadMain();

        std::mutex mutex;
        std::string name;
        std::vector<std::thread> threads;
        chrono_clock::duration flushSleepInterval;

        std::queue<shared_ptr<DispatchQueueWorkItemBase>> workQueue;
        std::condition_variable workReady;
        bool exit = false, dropPendingWork = false;
    };
} // namespace sp
