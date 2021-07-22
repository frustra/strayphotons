#include <tests.hh>

namespace ExampleTest {
    using namespace testing;

    void ExampleTest() {
        Assert(2 == (1 + 1), "This is an example of a test");
    }

    // Register the test
    Test test(&ExampleTest);
} // namespace ExampleTest
