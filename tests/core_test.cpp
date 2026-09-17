#include "tm/core.hpp"
#include <gtest/gtest.h>

TEST(CoreTest, ReturnZeroReturnsZero) {
    tm_core::Core core;
    EXPECT_EQ(core.returnZero(), 0);
}