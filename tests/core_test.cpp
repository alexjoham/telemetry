#include "tm/constants.hpp"
#include "tm/core.hpp"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>

TEST(CoreTest, ReturnZeroReturnsZero) {
    tm_core::Core core;
    EXPECT_EQ(core.returnZero(), 0);
}

// Deliberate UB. alignas makes the misalignment certain rather than luck.
TEST(SanitizerTest, MisalignedLoadIsCaught) {
    alignas(std::uint32_t) const std::array<std::uint8_t, tm_core::kMaxFrameSize> frame{};
    const auto *field = reinterpret_cast<const std::uint32_t *>(frame.data() + 1);
    EXPECT_EQ(*field, 0U);
}
