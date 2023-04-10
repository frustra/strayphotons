#include <tests.hh>

namespace ExampleTests {
    using namespace testing;

    void ExampleTest() {
        AssertEqual(2, 1 + 1, "This is an example of a test");
    }

    // Register the test
    Test test(&ExampleTest);
} // namespace ExampleTests
