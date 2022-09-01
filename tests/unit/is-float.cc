#include "core/Common.hh"

#include <tests.hh>

namespace IsFloatTests {
    using namespace testing;

    void TestIsFloatVariants() {
        AssertEqual(1.0f, std::stof("1."), "Expected '1.' to parse as 1.0");
        AssertEqual(0.01f, std::stof(".01"), "Expected '.01' to parse as 0.01");
        AssertEqual(-0.01f, std::stof("-.01"), "Expected '-.01' to parse as -0.01");

        Assert(!sp::is_float(""), "Expected '' to be invalid");
        Assert(!sp::is_float("-"), "Expected '-' to be invalid");
        Assert(sp::is_float("1."), "Expected '1.' to be valid");
        Assert(sp::is_float(".01"), "Expected '.01' to be valid");
        Assert(sp::is_float("-.01"), "Expected '-' to be valid");
        Assert(sp::is_float("-1"), "Expected '-' to be valid");
        Assert(!sp::is_float("1.0a"), "Expected '1.0a' to be invalid");
        Assert(sp::is_float("123"), "Expected '123' to be valid");
        Assert(sp::is_float("123.345"), "Expected '123.456' to be valid");
        Assert(sp::is_float("-123.345"), "Expected '-123.456' to be valid");
    }

    Test test1(&TestIsFloatVariants);
} // namespace IsFloatTests
