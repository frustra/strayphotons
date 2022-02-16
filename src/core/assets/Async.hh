#pragma once

#include "core/Common.hh"
#include "core/LockFreeMutex.hh"

#include <chrono>
#include <future>
#include <memory>
#include <mutex>

namespace sp {
    template<typename T>
    class Async {
    public:
        Async(std::future<std::shared_ptr<T>> &&future) : future(std::move(future)) {
            Assertf(this->future.valid(), "sp::Async created with invalid future");
        }
        Async(const std::shared_ptr<T> &ptr) : value(ptr) {
            valueSet.test_and_set();
        }

        bool Ready() {
            std::unique_lock lock(futureMutex, std::try_to_lock);
            if (valueSet.test()) return true;
            return lock && future.wait_for(std::chrono::seconds(0)) != std::future_status::timeout;
        }

        std::shared_ptr<const T> Get() {
            Assertf(this != nullptr, "AsyncPtr->Get() called on nullptr");
            if (!valueSet.test() && !futureWaiting.test_and_set()) {
                std::lock_guard lock(futureMutex);
                value = future.get();
                valueSet.test_and_set();
                valueSet.notify_all();
                return value;
            }

            while (!valueSet.test()) {
                valueSet.wait(false);
            }
            return value;
        }

    private:
        std::atomic_flag valueSet, futureWaiting;
        std::shared_ptr<const T> value;

        LockFreeMutex futureMutex;
        std::future<std::shared_ptr<T>> future;
    };

    template<typename T>
    using AsyncPtr = std::shared_ptr<Async<T>>;
} // namespace sp
