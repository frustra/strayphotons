#include "core/Common.hh"
#include "core/PreservingMap.hh"

#include <memory>
#include <tests.hh>
#include <thread>
#include <vector>

namespace PreservingMapTests {
    using namespace testing;

    sp::PreservingMap<int, 100> map;

    void TestPreservingMap() {
        {
            Timer t("Test preserving map");
            std::atomic_bool running = true;
            std::thread ticker([&] {
                while (running) {
                    map.Tick();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });

            std::vector<std::weak_ptr<int>> entries;
            for (int i = 0; i < 10; i++) {
                auto ptr = std::make_shared<int>(i);
                map.Register(std::to_string(i), ptr);
                entries.emplace_back(ptr);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            for (int i = 0; i < 10; i++) {
                auto ptr = map.Load(std::to_string(i));
                Assert(ptr != nullptr, "Expected entry to still exist after 10ms");
            }
            for (auto &weakPtr : entries) {
                auto ptr = weakPtr.lock();
                Assert(ptr != nullptr, "Expected entry to still exist after 10ms");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(190));

            for (int i = 0; i < 10; i++) {
                auto ptr = map.Load(std::to_string(i));
                Assert(!ptr, "Expected entry to have been cleaned up after 200ms");
            }
            for (auto &weakPtr : entries) {
                auto ptr = weakPtr.lock();
                Assert(!ptr, "Expected entry to have been cleaned up after 200ms");
            }
            running = false;
            ticker.join();
        }
    }

    Test test(&TestPreservingMap);
} // namespace PreservingMapTests
