/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "tests.hh"

#include "ecs/EcsImpl.hh"

#include <assets/AssetManager.hh>
#include <iostream>
#include <vector>

#ifndef TEST_TYPE
    #define TEST_TYPE "Unknown"
#endif
#include <ecs/SignalManager.hh>

namespace testing {
    std::vector<std::function<void()>> registeredTests;
} // namespace testing

using namespace testing;

int main(int argc, char **argv) {
    std::cout << "Running " << registeredTests.size() << " " << TEST_TYPE << " tests" << std::endl;

    sp::Assets().StartThread("../assets/");
    {
        auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
        auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
        stagingLock.Set<ecs::Signals>();
        liveLock.Set<ecs::Signals>();
    }
    {
        Timer t("Running tests");
        for (auto &test : registeredTests) {
            test();
            {
                // Reset the ECS between tests
                auto stagingLock = ecs::StartStagingTransaction<ecs::AddRemove>();
                auto liveLock = ecs::StartTransaction<ecs::AddRemove>();
                for (auto &ent : stagingLock.Entities()) {
                    ent.Destroy(stagingLock);
                }
                for (auto &ent : liveLock.Entities()) {
                    ent.Destroy(liveLock);
                }
                stagingLock.Unset<ecs::Signals>();
                liveLock.Set<ecs::Signals>();
            }
            auto &manager = ecs::GetSignalManager();
            while (manager.DropAllUnusedNodes() > 0) {}
            AssertEqual(manager.GetNodeCount(), 0u, "Expected no signal nodes after test");
        }
    }

    std::cout << "Tests complete" << std::endl << std::flush;
    std::cerr << std::flush;
    return 0;
}
