#include "core/Common.hh"
#include "core/InlineVector.hh"

#include <tests.hh>

namespace InlineVectorTests {
    using namespace testing;

    void TestInlineVectorWorksAfterMoveInContainer() {
        vector<sp::InlineVector<int, 4>> vec;

        for (int i = 0; i < 2; i++) {
            auto &v = vec.emplace_back();
            v.emplace_back(42 + i);
        }

        for (int i = 0; i < 2; i++) {
            AssertEqual(vec[i][0], 42 + i, "value at " + std::to_string(i) + " is not equal");
        }
    }

    void TestInlineVectorInsert() {
        sp::InlineVector<int, 8> vec;
        AssertTrue(vec.insert(vec.begin(), {1, 2}) == vec.begin(), "iterator unequal");
        AssertEqual(vec.size(), 2u);
        AssertEqual(vec[0], 1);
        AssertEqual(vec[1], 2);

        AssertTrue(vec.insert(vec.end(), {5, 6}) == vec.begin() + 2, "iterator unequal");
        AssertEqual(vec.size(), 4u);
        AssertEqual(vec[0], 1);
        AssertEqual(vec[1], 2);
        AssertEqual(vec[2], 5);
        AssertEqual(vec[3], 6);

        AssertTrue(vec.insert(vec.begin() + 2, {3, 4}) == vec.begin() + 2, "iterator unequal");
        AssertEqual(vec.size(), 6u);
        for (size_t i = 0; i < vec.size(); i++) {
            AssertEqual(vec[i], (int)i + 1);
        }
    }

    Test test1(&TestInlineVectorWorksAfterMoveInContainer);
    Test test2(&TestInlineVectorInsert);
} // namespace InlineVectorTests
