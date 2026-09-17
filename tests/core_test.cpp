#include "tm/core.hpp"
#include <gtest/gtest.h>

TEST(CoreTest, ReturnZeroReturnsZero) {
    Core core;
    EXPECT_EQ(core.returnZero(), 0);
}