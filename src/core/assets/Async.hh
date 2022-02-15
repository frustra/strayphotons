#pragma once

#include "core/Common.hh"

#include <chrono>
#include <future>
#include <memory>

namespace sp {
    template<typename T>
    class Async {
    public:
        Async(std::unique_ptr<T> &&ptr) : value(std::move(ptr)) {}
        Async(std::future<std::unique_ptr<T>> &&future) : future(std::move(future)) {}

        bool Ready() const {
            return value ||
                   (future.valid() && future.wait_for(std::chrono::seconds(0)) != std::future_status::deferred);
        }

        std::shared_ptr<const T> Get() {
            Assertf(this != nullptr, "AsyncPtr->Get() called on nullptr");
            if (!value && future.valid()) value = std::shared_ptr<const T>(future.get());
            return value;
        }

    private:
        std::shared_ptr<const T> value;
        std::future<std::unique_ptr<T>> future;
    };

    template<typename T>
    using AsyncPtr = std::shared_ptr<Async<T>>;
} // namespace sp
