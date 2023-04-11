#include "core/Common.hh"

#include <tests.hh>

namespace FloatHelperTests {
    using namespace testing;

    void TestIsFloatVariants() {
        AssertEqual(1.0f, std::stof("1."), "Expected '1.' to parse as 1.0");
        AssertEqual(0.01f, std::stof(".01"), "Expected '.01' to parse as 0.01");
        AssertEqual(-0.01f, std::stof("-.01"), "Expected '-.01' to parse as -0.01");

        AssertTrue(!sp::is_float(""), "Expected '' to be invalid");
        AssertTrue(!sp::is_float("foo"), "Expected 'foo' to be invalid");
        AssertTrue(!sp::is_float("-"), "Expected '-' to be invalid");
        AssertTrue(!sp::is_float("."), "Expected '.' to be invalid");
        AssertTrue(sp::is_float("1."), "Expected '1.' to be valid");
        AssertTrue(sp::is_float(".01"), "Expected '.01' to be valid");
        AssertTrue(sp::is_float("-.01"), "Expected '-' to be valid");
        AssertTrue(sp::is_float("-1"), "Expected '-' to be valid");
        AssertTrue(!sp::is_float("1.0a"), "Expected '1.0a' to be invalid");
        AssertTrue(!sp::is_float("1.0 2.0"), "Expected '1.0 2.0' to be invalid");
        AssertTrue(!sp::is_float("1.2.3.4"), "Expected '1.2.3.4' to be invalid");
        AssertTrue(sp::is_float("123"), "Expected '123' to be valid");
        AssertTrue(sp::is_float("123.345"), "Expected '123.456' to be valid");
        AssertTrue(sp::is_float("-123.345"), "Expected '-123.456' to be valid");
    }

    void TestFloat16Conversions() {
        AssertEqual<uint16_t>(sp::float16_t(0.0f), 0, "Expected to convert 0.0 (float32)");
        AssertEqual<uint16_t>(sp::float16_t(1.2345f), 0x3CF0, "Expected to convert 1.2345 (float32)");
        AssertEqual<uint16_t>(sp::float16_t(-0.0001f), 0x868D, "Expected to convert -0.00001 (float32)");
    }

    Test test1(&TestIsFloatVariants);
    Test test2(&TestFloat16Conversions);
} // namespace FloatHelperTests
