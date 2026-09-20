#include "test_frames.hpp"
#include "tlm/crc.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>

// The catalogue check value for CRC-16/IBM-3740 (CCITT-FALSE): poly 0x1021, init 0xFFFF,
// no reflection, no final xor. Pins the variant, not just the implementation.
constexpr std::array<std::byte, 9> kCheckInput{std::byte{'1'}, std::byte{'2'}, std::byte{'3'},
                                               std::byte{'4'}, std::byte{'5'}, std::byte{'6'},
                                               std::byte{'7'}, std::byte{'8'}, std::byte{'9'}};
static_assert(tlm::crc16(kCheckInput) == 0x29B1);

// Coverage is bytes 0..12+n-1, so 16 of the 18 frame bytes: the trailing CRC field is excluded.
constexpr std::span<const std::byte> kWorkedExampleCovered{
    std::span<const std::byte>{kWorkedExample}.first(16)};
static_assert(tlm::crc16(kWorkedExampleCovered) == 0x4A85);

TEST(CrcTest, CheckValueMatchesCatalogue) {
    EXPECT_EQ(tlm::crc16(kCheckInput), std::uint16_t{0x29B1});
}

TEST(CrcTest, WorkedExampleFrameMatchesSpec) {
    EXPECT_EQ(tlm::crc16(kWorkedExampleCovered), std::uint16_t{0x4A85});
}
