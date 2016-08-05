#include "gtest/gtest.h"

namespace test
{
	TEST(Example, ExampleTest)
	{
		ASSERT_TRUE(2 == (1 + 1)) << "This is an example of a test";
	}
}
