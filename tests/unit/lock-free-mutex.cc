/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "core/LockFreeMutex.hh"

#include <mutex>
#include <shared_mutex>
#include <tests.hh>
#include <thread>

namespace LockFreeMutexTests {
    using namespace testing;

    sp::LockFreeMutex mutex;

    void TestLockFreeMutex() {
        {
            Timer t("Try holding multiple shared locks");
            std::shared_lock lock1(mutex);
            std::shared_lock lock2(mutex);
            AssertEqual(mutex.try_lock(), false, "Shouldn't be able to get exclusive lock while shared lock is active");
        }
        {
            Timer t("Try holding exclusive lock");
            std::unique_lock lock(mutex);
            AssertEqual(mutex.try_lock_shared(),
                false,
                "Shouldn't be able to get shared lock while exclusive lock is active");
        }
        {
            Timer t("Try exclusive lock blocking on shared lock");
            mutex.lock_shared();
            mutex.lock_shared();
            std::atomic_bool locked = true;
            std::thread worker([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                locked = false;
                mutex.unlock_shared();
                mutex.unlock_shared();
            });
            std::unique_lock lock(mutex);
            AssertEqual(locked.load(), false, "Exclusive lock acquired before shared lock was released");
            worker.join();
        }
        {
            Timer t("Try shared lock blocking on exclusive lock");
            mutex.lock();
            std::atomic_bool locked = true;
            std::thread worker([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                locked = false;
                mutex.unlock();
            });
            std::shared_lock lock(mutex);
            AssertEqual(locked.load(), false, "Shared lock acquired before exclusive lock was released");
            worker.join();
        }
        {
            Timer t("Test continuous overlapping shared locks");

            std::thread blockingThread;
            std::vector<std::thread> readThreads;
            {
                for (size_t i = 0; i < 10; i++) {
                    // Start 10 overlapping shared lock threads
                    readThreads.emplace_back([i] {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10 * i));
                        mutex.lock_shared();
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        mutex.unlock_shared();
                    });
                }

                std::atomic_bool exclusiveAcquired = false;
                blockingThread = std::thread([&exclusiveAcquired] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    // Try to acquire an exclusive lock while continous shared locks are active
                    mutex.lock();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    mutex.unlock();
                    exclusiveAcquired = true;
                });

                while (!exclusiveAcquired) {
                    // Cycle through each shared lock and restart the thread when it completes
                    for (auto &thread : readThreads) {
                        thread.join();
                        thread = std::thread([] {
                            mutex.lock_shared();
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            mutex.unlock_shared();
                        });
                    }
                }
            }

            blockingThread.join();
            for (auto &thread : readThreads) {
                thread.join();
            }
        }
    }

    Test test(&TestLockFreeMutex);
} // namespace LockFreeMutexTests
