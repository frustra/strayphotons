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
    }

    Test test(&TestLockFreeMutex);
} // namespace LockFreeMutexTests
