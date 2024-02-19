/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "common/PreservingMap.hh"

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
                AssertTrue(ptr != nullptr, "Expected entry to still exist after 100ms");
            }
            for (auto &weakPtr : entries) {
                auto ptr = weakPtr.lock();
                AssertTrue(ptr != nullptr, "Expected entry to still exist after 100ms");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            map.Tick(std::chrono::milliseconds(100));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            map.Tick(std::chrono::milliseconds(1));

            for (int i = 0; i < 10; i++) {
                auto ptr = map.Load(std::to_string(i));
                AssertTrue(!ptr, "Expected entry to have been cleaned up after 101ms");
            }
            for (auto &weakPtr : entries) {
                auto ptr = weakPtr.lock();
                AssertTrue(!ptr, "Expected entry to have been cleaned up after 101ms");
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            map.Tick(std::chrono::milliseconds(10));
        }
    }

    Test test(&TestPreservingMap);
} // namespace PreservingMapTests
