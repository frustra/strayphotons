#include "core/Common.hh"
#include "core/PreservingMap.hh"

#include <memory>
#include <tests.hh>
#include <thread>
#include <vector>

namespace PreservingMapTests {
    using namespace testing;

    sp::PreservingMap<std::string, int, 100> map;

    void TestPreservingMap() {
        {
            Timer t("Test preserving map");
            map.Tick(std::chrono::milliseconds(1));

            std::vector<std::weak_ptr<int>> entries;
            for (int i = 0; i < 10; i++) {
                auto ptr = std::make_shared<int>(i);
                map.Register(std::to_string(i), ptr);
                entries.emplace_back(ptr);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            map.Tick(std::chrono::milliseconds(50));
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            map.Tick(std::chrono::milliseconds(50));

            for (int i = 0; i < 10; i++) {
                auto ptr = map.Load(std::to_string(i));
                Assert(ptr != nullptr, "Expected entry to still exist after 100ms");
            }
            for (auto &weakPtr : entries) {
                auto ptr = weakPtr.lock();
                Assert(ptr != nullptr, "Expected entry to still exist after 100ms");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            map.Tick(std::chrono::milliseconds(100));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            map.Tick(std::chrono::milliseconds(1));

            for (int i = 0; i < 10; i++) {
                auto ptr = map.Load(std::to_string(i));
                Assert(!ptr, "Expected entry to have been cleaned up after 101ms");
            }
            for (auto &weakPtr : entries) {
                auto ptr = weakPtr.lock();
                Assert(!ptr, "Expected entry to have been cleaned up after 101ms");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            map.Tick(std::chrono::milliseconds(10));
        }
    }

    Test test(&TestPreservingMap);
} // namespace PreservingMapTests
