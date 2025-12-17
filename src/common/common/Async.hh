/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "common/LockFreeMutex.hh"
#include "common/Logging.hh"

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <type_traits>

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

    template<typename T, typename... Args>
    inline AsyncPtr<T> make_async(Args &&...args) {
        return make_shared<Async<T>>(std::make_shared<T>(std::forward<Args>(args)...));
    }
} // namespace sp
