#include "tests.hh"

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
        }
    }

    std::cout << "Tests complete" << std::endl << std::flush;
    std::cerr << std::flush;
    return 0;
}
