#include "tm/core.hpp"
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <string_view>

TEST(CoreTest, CorrectCrcCalculationForKnownHeader) {
    tm_core::Core core;
    constexpr std::string_view kCheck{"123456789"};
    const auto data = std::as_bytes(std::span{kCheck});
    EXPECT_EQ(core.crc16(data), 0x29B1);
}

TEST(CoreTest, CorrectCrcCalculationForWorkedExample) {
    tm_core::Core core;
    // The worked example frame from docs/format.md without its trailing CRC field: coverage is bytes 0..12+n-1, so 16 of the 18 frame bytes.
    constexpr std::array<std::byte, 16> kFrame{
        std::byte{0xA5}, std::byte{0xC3}, std::byte{0x01}, std::byte{0x01},
        std::byte{0x41}, std::byte{0x9C}, std::byte{0x4D}, std::byte{0x3C},
        std::byte{0x2B}, std::byte{0x1A}, std::byte{0x04}, std::byte{0x00},
        std::byte{0xEB}, std::byte{0x68}, std::byte{0x7D}, std::byte{0x32}};
    EXPECT_EQ(core.crc16(kFrame), 0x4A85);
}
