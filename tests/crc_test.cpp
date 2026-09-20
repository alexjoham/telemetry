#include "tlm/crc.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <string_view>

TEST(CrcTest, CheckValueMatchesCatalogue) {
    // The catalogue check value for CRC-16/IBM-3740 (CCITT-FALSE): poly 0x1021, init 0xFFFF,
    // no reflection, no final xor. Pins the variant, not just the implementation.
    constexpr std::string_view kCheck{"123456789"};
    const auto data = std::as_bytes(std::span{kCheck});
    EXPECT_EQ(tlm::crc16(data), std::uint16_t{0x29B1});
}

TEST(CrcTest, WorkedExampleFrameMatchesSpec) {
    // The worked example frame from docs/format.md without its trailing CRC field: coverage is
    // bytes 0..12+n-1, so 16 of the 18 frame bytes.
    constexpr std::array<std::byte, 16> kFrame{
        std::byte{0xA5}, std::byte{0xC3}, std::byte{0x01}, std::byte{0x01},
        std::byte{0x41}, std::byte{0x9C}, std::byte{0x4D}, std::byte{0x3C},
        std::byte{0x2B}, std::byte{0x1A}, std::byte{0x04}, std::byte{0x00},
        std::byte{0xEB}, std::byte{0x68}, std::byte{0x7D}, std::byte{0x32}};
    EXPECT_EQ(tlm::crc16(kFrame), std::uint16_t{0x4A85});
}
