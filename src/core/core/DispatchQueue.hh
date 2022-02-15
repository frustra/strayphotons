#pragma once

#include "assets/Async.hh"
#include "core/Common.hh"
#include "core/Tracing.hh"

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
        template<typename T>
        class Future {};

        template<typename T>
        class Future<std::future<T>> {
        public:
            typedef T ReturnType;

            Future(std::future<T> &future) : future(std::move(future)) {}

            bool Ready() const {
                return future.wait_for(std::chrono::seconds(0)) != std::future_status::timeout;
            }

            ReturnType Get() {
                return future.get();
            }

        private:
            std::future<T> future;
        };

        template<typename T>
        struct Future<std::shared_future<T>> {
        public:
            typedef T ReturnType;

            Future(const std::shared_future<T> &future) : future(future) {}

            bool Ready() const {
                return future.wait_for(std::chrono::seconds(0)) != std::future_status::timeout;
            }

            ReturnType Get() {
                return future.get();
            }

        private:
            std::shared_future<T> future;
        };

        template<typename T>
        struct Future<AsyncPtr<T>> {
        public:
            typedef std::shared_ptr<const T> ReturnType;

            Future(const AsyncPtr<T> &future) : future(future) {}

            bool Ready() const {
                return !future || future->Ready();
            }

            ReturnType Get() {
                if (!future) return nullptr;
                return future->Get();
            }

        private:
            AsyncPtr<T> future;
        };

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
    template<typename ReturnType, typename Fn, typename... Futures>
    struct DispatchQueueWorkItem final : public DispatchQueueWorkItemBase {
        using FutureTuple = std::tuple<detail::Future<Futures>...>;
        using ResultTuple = std::tuple<typename detail::Future<Futures>::ReturnType...>;

        template<typename... Args>
        DispatchQueueWorkItem(Fn &&func, Args &&...args)
            : func(std::move(func)),
              waitForFutures(std::make_tuple(detail::Future<std::remove_cvref_t<Args>>(args)...)) {}

        std::promise<ReturnType> promise;
        Fn func;
        FutureTuple waitForFutures;

        void Process() {
            ZoneScoped;
            ResultTuple args;
            {
                ZoneScopedN("ResolveFutures");
                args = std::apply(
                    [](auto &&...x) {
                        return std::make_tuple(x.Get()...);
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
            return detail::constexpr_for<0, sizeof...(Futures)>([this]<int I>() {
                return std::get<I>(waitForFutures).Ready();
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
        template<typename ReturnType, typename Fn, typename... Futures>
        std::future<ReturnType> Dispatch(Fn &&func, Futures &&...futures) {
            Assert(!exit, "tried to dispatch to a shut down queue");
            auto item = make_shared<DispatchQueueWorkItem<ReturnType, Fn, std::remove_cvref_t<Futures>...>>(
                std::move(func),
                std::move(futures)...);

            std::unique_lock<std::mutex> lock(mutex);
            auto fut = item->promise.get_future();
            workQueue.push(item);
            workReady.notify_one();
            return fut;
        }

    private:
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
