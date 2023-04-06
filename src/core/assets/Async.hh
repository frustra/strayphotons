#pragma once

#include "core/Common.hh"
#include "core/Logging.hh"
#include "core/LockFreeMutex.hh"

#include <chrono>
#include <future>
#include <memory>
#include <mutex>

namespace sp {
    template<typename T>
    class Async {
    public:
        Async() {}
        Async(const std::shared_ptr<T> &ptr) : value(ptr) {
            valid.test_and_set();
        }

        bool Ready() const {
            return valid.test();
        }

        std::shared_ptr<T> Get() const {
            Assertf(this != nullptr, "AsyncPtr->Get() called on nullptr");
            while (!valid.test()) {
                valid.wait(false);
            }
            return value;
        }

        void Set(const std::shared_ptr<T> &ptr) {
            value = ptr;
            Assert(!valid.test_and_set(), "Async::Set called multiple times");
            valid.notify_all();
        }

    private:
        std::atomic_flag valid;
        std::shared_ptr<T> value;
    };

    template<typename T>
    using AsyncPtr = std::shared_ptr<Async<T>>;
} // namespace sp
