#include "tlm/crc.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

// The catalogue check value for CRC-16/IBM-3740 (CCITT-FALSE): poly 0x1021, init 0xFFFF,
// no reflection, no final xor. Pins the variant, not just the implementation.
constexpr std::array<std::byte, 9> kCheckInput{std::byte{'1'}, std::byte{'2'}, std::byte{'3'},
                                               std::byte{'4'}, std::byte{'5'}, std::byte{'6'},
                                               std::byte{'7'}, std::byte{'8'}, std::byte{'9'}};
static_assert(tlm::crc16(kCheckInput) == 0x29B1);

// The worked example frame from docs/format.md without its trailing CRC field: coverage is
// bytes 0..12+n-1, so 16 of the 18 frame bytes.
constexpr std::array<std::byte, 16> kWorkedExample{
    std::byte{0xA5}, std::byte{0xC3}, std::byte{0x01}, std::byte{0x01},
    std::byte{0x41}, std::byte{0x9C}, std::byte{0x4D}, std::byte{0x3C},
    std::byte{0x2B}, std::byte{0x1A}, std::byte{0x04}, std::byte{0x00},
    std::byte{0xEB}, std::byte{0x68}, std::byte{0x7D}, std::byte{0x32}};
static_assert(tlm::crc16(kWorkedExample) == 0x4A85);

TEST(CrcTest, CheckValueMatchesCatalogue) {
    EXPECT_EQ(tlm::crc16(kCheckInput), std::uint16_t{0x29B1});
}

TEST(CrcTest, WorkedExampleFrameMatchesSpec) {
    EXPECT_EQ(tlm::crc16(kWorkedExample), std::uint16_t{0x4A85});
}
