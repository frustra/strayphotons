#include "gtest/gtest.h"

// Shows simple usage of google test
// TODO-cs: remove this file once real tests are added
TEST(SampleTestCase, Test1) {
  EXPECT_EQ(1, 1);
}

TEST(SampleTestCase, Test2) {
  EXPECT_EQ(2, 2);
}

TEST(SampleTestCase2, SomeTestName) {
  EXPECT_EQ(1, 1);
}
