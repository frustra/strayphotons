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
        Async(std::future<std::shared_ptr<T>> &&future) : future(future.share()) {
            Assertf(this->future.valid(), "sp::Async created with invalid future");
        }
        Async(const std::shared_ptr<T> &ptr) {
            future = std::async(std::launch::deferred, [ptr] {
                return ptr;
            }).share();
        }

        bool Ready() const {
            return future.wait_for(std::chrono::seconds(0)) != std::future_status::timeout;
        }

        std::shared_ptr<T> Get() {
            Assertf(this != nullptr, "AsyncPtr->Get() called on nullptr");
            return future.get();
        }

    private:
        std::shared_future<std::shared_ptr<T>> future;
    };

    template<typename T>
    using AsyncPtr = std::shared_ptr<Async<T>>;
} // namespace sp
