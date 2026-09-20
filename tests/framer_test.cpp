#include "tlm/framer.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <variant>

// The worked example frame from docs/format.md, all 18 bytes: L = 12 + 4 + 2.
// The only expected length in this file that comes from the spec rather than from the rules.
constexpr std::array<std::byte, 18> kWorkedExample{
    std::byte{0xA5}, std::byte{0xC3}, std::byte{0x01}, std::byte{0x01}, std::byte{0x41},
    std::byte{0x9C}, std::byte{0x4D}, std::byte{0x3C}, std::byte{0x2B}, std::byte{0x1A},
    std::byte{0x04}, std::byte{0x00}, std::byte{0xEB}, std::byte{0x68}, std::byte{0x7D},
    std::byte{0x32}, std::byte{0x85}, std::byte{0x4A}};

constexpr std::size_t kWorkedExampleLength = 18;

TEST(FramerTest, WorkedExampleAloneIsFound) {
    const tlm::FrameResult result = tlm::frame(kWorkedExample);
    ASSERT_TRUE(std::holds_alternative<tlm::Found>(result));
    EXPECT_EQ(std::get<tlm::Found>(result).length, kWorkedExampleLength);
}

TEST(FramerTest, EmptyBufferIsIncomplete) {
    const tlm::FrameResult result = tlm::frame({});
    EXPECT_TRUE(std::holds_alternative<tlm::Incomplete>(result));
}

// Six bytes: the framer cannot reach the length field at offset 10, so it answers without
// having read a length at all. Distinct path from HeaderWithoutPayloadIsIncomplete.
TEST(FramerTest, SyncWordWithoutLengthFieldIsIncomplete) {
    const tlm::FrameResult result = tlm::frame(std::span<const std::byte>{kWorkedExample}.first(6));
    EXPECT_TRUE(std::holds_alternative<tlm::Incomplete>(result));
}

// Twelve bytes: the length field is readable. The framer reads 4, computes L = 18, has 12.
TEST(FramerTest, HeaderWithoutPayloadIsIncomplete) {
    const tlm::FrameResult result =
        tlm::frame(std::span<const std::byte>{kWorkedExample}.first(12));
    EXPECT_TRUE(std::holds_alternative<tlm::Incomplete>(result));
}

TEST(FramerTest, BufferWithoutSyncWordIsDiscardedWhole) {
    constexpr std::array<std::byte, 10> kNoise{};
    const tlm::FrameResult result = tlm::frame(kNoise);
    ASSERT_TRUE(std::holds_alternative<tlm::Discard>(result));
    EXPECT_EQ(std::get<tlm::Discard>(result).count, std::size_t{10});
}

// Pins the two-call contract: a run of garbage in front of a frame never comes back as a
// single found-at-an-offset result. See 0001's rejected alternatives.
TEST(FramerTest, GarbageBeforeFrameDiscardsFirstAndFindsSecond) {
    constexpr std::size_t kGarbage = 5;
    std::array<std::byte, kGarbage + kWorkedExampleLength> buffer{
        std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44}, std::byte{0x55}};
    std::copy(kWorkedExample.begin(), kWorkedExample.end(), buffer.begin() + kGarbage);

    const tlm::FrameResult first = tlm::frame(buffer);
    ASSERT_TRUE(std::holds_alternative<tlm::Discard>(first));
    EXPECT_EQ(std::get<tlm::Discard>(first).count, kGarbage);

    const tlm::FrameResult second =
        tlm::frame(std::span<const std::byte>{buffer}.subspan(kGarbage));
    ASSERT_TRUE(std::holds_alternative<tlm::Found>(second));
    EXPECT_EQ(std::get<tlm::Found>(second).length, kWorkedExampleLength);
}

// The trailing 0xA5 may be the first byte of a sync word whose 0xC3 has not arrived.
// Discarding it would make the frame behind it unrecognisable forever.
TEST(FramerTest, TrailingSyncByteIsNotDiscarded) {
    constexpr std::array<std::byte, 3> kBuffer{std::byte{0x00}, std::byte{0x00}, std::byte{0xA5}};
    const tlm::FrameResult result = tlm::frame(kBuffer);
    ASSERT_TRUE(std::holds_alternative<tlm::Discard>(result));
    EXPECT_EQ(std::get<tlm::Discard>(result).count, std::size_t{2});
}

// A buffer that is nothing but a partial sync word has no bytes to discard. Discard{0}
// would spin the caller's drop-and-call-again loop, so the answer is Incomplete.
TEST(FramerTest, LoneSyncByteIsIncomplete) {
    constexpr std::array<std::byte, 1> kBuffer{std::byte{0xA5}};
    const tlm::FrameResult result = tlm::frame(kBuffer);
    EXPECT_TRUE(std::holds_alternative<tlm::Incomplete>(result));
}
