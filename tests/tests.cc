#include "tests.hh"

#include "ecs/EcsImpl.hh"

#include <iostream>
#include <vector>

#ifndef TEST_TYPE
    #define TEST_TYPE "Unknown"
#endif

namespace testing {
    std::vector<std::function<void()>> registeredTests;
} // namespace testing

using namespace testing;

int main(int argc, char **argv) {
    std::cout << "Running " << registeredTests.size() << " " << TEST_TYPE << " tests" << std::endl;
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
            }
        }
    }

    std::cout << "Tests complete" << std::endl << std::flush;
    std::cerr << std::flush;
    return 0;
}
