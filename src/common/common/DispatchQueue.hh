/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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
#include <type_traits>
#include <vector>

namespace sp {
    namespace detail {
        template<typename... T, std::size_t... I>
        constexpr auto subtuple(std::tuple<T...> &&t, std::index_sequence<I...>) {
            return std::forward_as_tuple(std::get<I>(t)...);
        }

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
            typedef std::shared_ptr<T> ReturnType;

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
    } // namespace detail

    struct DispatchQueueWorkItemBase {
        virtual void Process() = 0;
        virtual bool Ready() = 0;
    };

    class DispatchQueue;

    template<typename ReturnType, typename Fn, typename... Futures>
    struct DispatchQueueWorkItem final : public DispatchQueueWorkItemBase {
        using FutureTuple = std::tuple<detail::Future<Futures>...>;
        using ResultTuple = std::tuple<typename detail::Future<Futures>::ReturnType...>;

        template<typename... Args>
        DispatchQueueWorkItem(DispatchQueue &queue, Fn &&func, Args &&...args)
            : queue(queue), returnValue(std::make_shared<Async<ReturnType>>()), func(std::move(func)),
              waitForFutures(std::make_tuple(detail::Future<std::remove_cvref_t<Args>>(args)...)) {}

        DispatchQueue &queue;
        AsyncPtr<ReturnType> returnValue;
        Fn func;
        FutureTuple waitForFutures;

        void Process();

        bool Ready() {
            return std::apply(
                [](auto &&...future) {
                    return (future.Ready() && ...);
                },
                waitForFutures);
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
         * Supported input types are std::future, std::shared_future, and sp::AsyncPtr
         *
         * If the return value of the function is itself a future, the future returned
         * from Dispatch will be set to its value once it is ready.
         *
         * Usage: Dispatch<R>(FutureType<T>..., [](T...) { return std::make_shared<R>(); });
         * Example:
         *  auto image = queue.Dispatch<Image>([]() { return std::make_shared<Image>(); });
         *  queue.Dispatch<void>(image, [](std::shared_ptr<Image> image) { });
         */
        template<typename ReturnType, typename... FuturesAndFn>
        AsyncPtr<ReturnType> Dispatch(FuturesAndFn &&...args) {
            // if C++ adds support for parameters after a parameter pack, delete this function
            const size_t lastArg = sizeof...(FuturesAndFn) - 1;
            auto tupl = std::make_tuple(std::move(args)...);
            auto fn = std::move(std::get<lastArg>(tupl));
            auto futures = detail::subtuple(std::move(tupl), std::make_index_sequence<lastArg>());
            return std::apply(
                [&](auto &&...futures) {
                    return DispatchInternal<ReturnType>(std::move(fn), std::move(futures)...);
                },
                futures);
        }

        /**
         * When `from` is ready, its value will be set in `to`
         */
        template<typename FutT, typename T>
        void ForwardAsync(FutT from, const AsyncPtr<T> &to) {
            if (from->Ready()) {
                to->Set(from->Get());
            } else {
                Dispatch<void>(from, [to](auto &fromValue) {
                    to->Set(fromValue);
                });
            }
        }

        template<typename ReturnType, typename Fn, typename... Futures>
        AsyncPtr<ReturnType> DispatchInternal(Fn &&func, Futures &&...futures) {
            Assert(!exit, "tried to dispatch to a shut down queue");
            auto item = make_shared<DispatchQueueWorkItem<ReturnType, Fn, std::remove_cvref_t<Futures>...>>(*this,
                std::move(func),
                std::move(futures)...);

            std::unique_lock<std::mutex> lock(mutex);
            workQueue.push(item);
            workReady.notify_one();
            return item->returnValue;
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

    template<typename ReturnType, typename Fn, typename... Futures>
    void DispatchQueueWorkItem<ReturnType, Fn, Futures...>::Process() {
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
            returnValue->Set(nullptr);
        } else {
            auto result = std::apply(func, args);
            if constexpr (std::is_constructible<detail::Future<decltype(result)>, decltype(result)>()) {
                queue.ForwardAsync(result, returnValue);
            } else {
                returnValue->Set(result);
            }
        }
    }
} // namespace sp
