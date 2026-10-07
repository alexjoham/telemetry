#include "test_frames.hpp"
#include "tlm/constants.hpp"
#include "tlm/crc.hpp"
#include "tlm/decoder.hpp"
#include "tlm/error.hpp"
#include "tlm/framer.hpp"
#include "tlm/stream.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace {

struct Pulled {
    std::vector<tlm::Event> events;
    std::vector<std::size_t> consumed_after;
};

[[nodiscard]] Pulled pullAll(std::span<const std::byte> bytes, std::size_t frame_bytes) {
    tlm::Stream stream{bytes};
    Pulled pulled;
    std::size_t dropped = 0;
    while (const std::optional<tlm::Event> event = stream.next()) {
        if (pulled.events.size() == bytes.size()) {
            ADD_FAILURE() << "more events than bytes, the stream is stuck";
            break;
        }

        if (const auto *const discard = std::get_if<tlm::Discard>(&*event)) {
            dropped += discard->count;
        } else if (const auto *const resync = std::get_if<tlm::Resync>(&*event)) {
            dropped += resync->count;
        } else if (const auto *const error = std::get_if<tlm::Error>(&*event)) {
            EXPECT_NE(error->code, tlm::ErrorCode::BadChecksum);
            EXPECT_NE(error->code, tlm::ErrorCode::MalformedFrame);
        }

        pulled.events.push_back(*event);
        pulled.consumed_after.push_back(stream.consumed());
    }

    EXPECT_EQ(frame_bytes + dropped, stream.consumed());
    return pulled;
}

// A5 C3, then zeros. The length field (byte 10) is 0, so the framer claims L = 14, and the zero
// CRC field makes only the CRC check reject it. The worked example follows, so the 12 zeros between
// the two sync words are the Discard.
constexpr std::size_t kFalseStartSize = tlm::kFixedHeaderSize + tlm::kCrcSize;
constexpr std::size_t kFalseStartStreamSize = kFalseStartSize + kWorkedExampleLength;

[[nodiscard]] constexpr std::array<std::byte, kFalseStartStreamSize> makeFalseStartStream() {
    std::array<std::byte, kFalseStartStreamSize> stream{};
    stream[0] = tlm::kSyncByte0;
    stream[1] = tlm::kSyncByte1;
    std::copy(kWorkedExample.begin(), kWorkedExample.end(),
              stream.begin() + static_cast<std::ptrdiff_t>(kFalseStartSize));
    return stream;
}

constexpr std::array<std::byte, kFalseStartStreamSize> kFalseStartStream = makeFalseStartStream();

// The framer must claim exactly the false start's size, or the Discard below would not be 12.
static_assert(tlm::kFixedHeaderSize +
                  std::to_integer<std::size_t>(kFalseStartStream[tlm::kLengthFieldOffset]) +
                  tlm::kCrcSize ==
              kFalseStartSize);
// The stored CRC is zero, so a computed CRC of zero would make this decode instead of reject.
static_assert(
    tlm::crc16(std::span<const std::byte>{kFalseStartStream}.first(tlm::kFixedHeaderSize)) != 0);

// A rejected frame followed by the worked example, so a test sees the stream skip the first and
// decode the second. The size is a parameter because the wrong-length frame is not 18 bytes.
template <std::size_t N>
[[nodiscard]] constexpr std::array<std::byte, N + kWorkedExampleLength>
followedByWorkedExample(const std::array<std::byte, N> &rejected) {
    std::array<std::byte, N + kWorkedExampleLength> buffer{};
    std::copy(rejected.begin(), rejected.end(), buffer.begin());
    std::copy(kWorkedExample.begin(), kWorkedExample.end(),
              buffer.begin() + static_cast<std::ptrdiff_t>(N));
    return buffer;
}

} // namespace

TEST(StreamTest, WorkedExampleAloneYieldsOneDecodedFrameThenNullopt) {
    const Pulled pulled = pullAll(kWorkedExample, kWorkedExampleLength);

    ASSERT_EQ(pulled.events.size(), 1U);
    const auto *const decoded = std::get_if<tlm::DecodedFrame>(&pulled.events[0]);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->frame_header.sequence, std::uint16_t{40001});
    EXPECT_EQ(decoded->vehicle_state.steering_count, std::uint16_t{1443});

    const std::vector<std::size_t> expected_consumed{kWorkedExampleLength};
    EXPECT_EQ(pulled.consumed_after, expected_consumed);
}

TEST(StreamTest, GarbageBeforeFrameYieldsDiscardThenFrame) {
    constexpr std::size_t kGarbage = 5;
    std::array<std::byte, kGarbage + kWorkedExampleLength> buffer{
        std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44}, std::byte{0x55}};
    std::copy(kWorkedExample.begin(), kWorkedExample.end(), buffer.begin() + kGarbage);

    const Pulled pulled = pullAll(buffer, kWorkedExampleLength);
    ASSERT_EQ(pulled.events.size(), 2U);

    const auto *const discard = std::get_if<tlm::Discard>(&pulled.events[0]);
    ASSERT_NE(discard, nullptr);
    EXPECT_EQ(discard->count, kGarbage);
    EXPECT_EQ(pulled.consumed_after[0], kGarbage);

    const auto *const decoded = std::get_if<tlm::DecodedFrame>(&pulled.events[1]);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->frame_header.sequence, std::uint16_t{40001});
    EXPECT_EQ(decoded->vehicle_state.steering_count, std::uint16_t{1443});
    EXPECT_EQ(pulled.consumed_after[1], kWorkedExampleLength + kGarbage);
}

TEST(StreamTest, FalseSyncWordYieldsResyncThenDiscardThenFrame) {
    const Pulled pulled = pullAll(kFalseStartStream, kWorkedExampleLength);
    ASSERT_EQ(pulled.events.size(), 3U);

    // The shift and the discard are literals: the constant is the value under test.
    const auto *const resync = std::get_if<tlm::Resync>(&pulled.events[0]);
    ASSERT_NE(resync, nullptr);
    EXPECT_EQ(resync->error.code, tlm::ErrorCode::BadChecksum);
    EXPECT_EQ(resync->count, 2U);

    const auto *const discard = std::get_if<tlm::Discard>(&pulled.events[1]);
    ASSERT_NE(discard, nullptr);
    EXPECT_EQ(discard->count, 12U);

    const auto *const decoded = std::get_if<tlm::DecodedFrame>(&pulled.events[2]);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->frame_header.sequence, std::uint16_t{40001});
    EXPECT_EQ(decoded->vehicle_state.steering_count, std::uint16_t{1443});

    const std::vector<std::size_t> expected_consumed{2, kFalseStartSize, kFalseStartStreamSize};
    EXPECT_EQ(pulled.consumed_after, expected_consumed);
}

// The flip is in frame 1's payload, so the framer still claims 18 bytes and only the CRC rejects
// them. Frame 2 differs in sequence, so decoding frame 1 by mistake cannot pass for recovery.
TEST(StreamTest, CorruptedFrameIsDroppedAndTheNextOneDecoded) {
    constexpr auto kFirst = WithValidCrc(kWorkedExample);
    constexpr auto kSecond = [] {
        auto f = kWorkedExample;
        f[tlm::kSequenceNumberOffset] = std::byte{0x42};
        return WithValidCrc(f);
    }();
    constexpr auto kBuffer = [&] {
        std::array<std::byte, 2 * kWorkedExampleLength> buffer{};
        std::copy(kFirst.begin(), kFirst.end(), buffer.begin());
        std::copy(kSecond.begin(), kSecond.end(),
                  buffer.begin() + static_cast<std::ptrdiff_t>(kWorkedExampleLength));
        buffer[tlm::kFixedHeaderSize] ^= std::byte{0xFF};
        return buffer;
    }();

    // pullAll stops only on nullopt, so three events means next() returned nullopt after the third.
    const Pulled pulled = pullAll(kBuffer, kWorkedExampleLength);
    ASSERT_EQ(pulled.events.size(), 3U);

    const auto *const resync = std::get_if<tlm::Resync>(&pulled.events[0]);
    ASSERT_NE(resync, nullptr);
    EXPECT_EQ(resync->error.code, tlm::ErrorCode::BadChecksum);
    EXPECT_EQ(resync->count, 2U);

    // The rest of frame 1 after the shift: no A5 in it, so the next candidate is frame 2's sync.
    const auto *const discard = std::get_if<tlm::Discard>(&pulled.events[1]);
    ASSERT_NE(discard, nullptr);
    EXPECT_EQ(discard->count, 16U);

    const auto *const decoded = std::get_if<tlm::DecodedFrame>(&pulled.events[2]);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->frame_header.sequence, std::uint16_t{40002});

    const std::vector<std::size_t> expected_consumed{2, 18, 36};
    EXPECT_EQ(pulled.consumed_after, expected_consumed);
    EXPECT_EQ(pulled.consumed_after.back(), kBuffer.size());
}

TEST(StreamTest, UnsupportedVersionIsSkippedWithoutResyncOrDiscard) {
    constexpr auto kRejected = [] {
        auto f = kWorkedExample;
        f[tlm::kVersionOffset] = std::byte{0x02};
        return WithValidCrc(f);
    }();

    const auto buffer = followedByWorkedExample(kRejected);

    const Pulled pulled = pullAll(buffer, buffer.size());
    ASSERT_EQ(pulled.events.size(), 2U);

    const auto *const error = std::get_if<tlm::Error>(&pulled.events[0]);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::UnsupportedVersion);
    EXPECT_EQ(error->detail, std::uint8_t{0x02});
    EXPECT_EQ(pulled.consumed_after[0], kRejected.size());

    const auto *const decoded = std::get_if<tlm::DecodedFrame>(&pulled.events[1]);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->frame_header.sequence, std::uint16_t{40001});
    EXPECT_EQ(decoded->vehicle_state.steering_count, std::uint16_t{1443});
    EXPECT_EQ(pulled.consumed_after[1], buffer.size());
}

TEST(StreamTest, UnknownMessageIdIsSkippedWithoutResyncOrDiscard) {
    constexpr auto kRejected = [] {
        auto f = kWorkedExample;
        f[tlm::kMessageIdOffset] = std::byte{0x02};
        return WithValidCrc(f);
    }();

    const auto buffer = followedByWorkedExample(kRejected);

    const Pulled pulled = pullAll(buffer, buffer.size());
    ASSERT_EQ(pulled.events.size(), 2U);

    const auto *const error = std::get_if<tlm::Error>(&pulled.events[0]);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::UnknownMessageId);
    EXPECT_EQ(error->detail, std::uint8_t{0x02});
    EXPECT_EQ(pulled.consumed_after[0], kRejected.size());

    const auto *const decoded = std::get_if<tlm::DecodedFrame>(&pulled.events[1]);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->frame_header.sequence, std::uint16_t{40001});
    EXPECT_EQ(decoded->vehicle_state.steering_count, std::uint16_t{1443});
    EXPECT_EQ(pulled.consumed_after[1], buffer.size());
}

TEST(StreamTest, WrongPayloadLengthIsSkippedWithoutResyncOrDiscard) {
    // One byte over the VehicleState payload: 19 bytes, not 18, with the header copied unchanged.
    constexpr std::size_t kPayloadSize = tlm::kVehicleStatePayloadSize + 1;
    constexpr auto kRejected = [] {
        std::array<std::byte, tlm::kFixedHeaderSize + kPayloadSize + tlm::kCrcSize> f{};
        std::copy_n(kWorkedExample.begin(), tlm::kFixedHeaderSize, f.begin());
        f[tlm::kLengthFieldOffset] = static_cast<std::byte>(kPayloadSize);
        return WithValidCrc(f);
    }();

    const auto buffer = followedByWorkedExample(kRejected);

    const Pulled pulled = pullAll(buffer, buffer.size());
    ASSERT_EQ(pulled.events.size(), 2U);

    const auto *const error = std::get_if<tlm::Error>(&pulled.events[0]);
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::WrongPayloadLength);
    EXPECT_EQ(error->detail, std::uint8_t{0x01});
    EXPECT_EQ(pulled.consumed_after[0], kRejected.size());

    const auto *const decoded = std::get_if<tlm::DecodedFrame>(&pulled.events[1]);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->frame_header.sequence, std::uint16_t{40001});
    EXPECT_EQ(decoded->vehicle_state.steering_count, std::uint16_t{1443});
    EXPECT_EQ(pulled.consumed_after[1], buffer.size());
}

TEST(StreamTest, TruncatedFrameYieldsNulloptAndConsumesNothing) {
    constexpr std::size_t kTruncated = kWorkedExampleLength - 1;

    // frame_bytes is 0, so pullAll's accounting also asserts that consumed() stayed at 0.
    const Pulled pulled = pullAll(std::span<const std::byte>{kWorkedExample}.first(kTruncated), 0);

    EXPECT_TRUE(pulled.events.empty());
}

// Layout, with the offset each piece starts at:
//   0 garbage (3) | 3 good frame | 21 payload flipped | 39 garbage (3) | 42 false start (14)
//   | 56 unknown-id frame | 74 good frame | 92 partial frame (17)
// The flip is in the payload, so the framer still sees L = 18 and only the CRC rejects it. The
// false start is the one from the test above, with its claimed 14 bytes inside the buffer.
TEST(StreamTest, MixedStreamYieldsTheExactEventSequence) {
    constexpr std::size_t kLeadingGarbage = 3;
    constexpr std::size_t kInnerGarbage = 3;
    constexpr std::size_t kPartialSize = kWorkedExampleLength - 1;
    constexpr std::size_t kMixedSize = kLeadingGarbage + (4 * kWorkedExampleLength) +
                                       kInnerGarbage + kFalseStartSize + kPartialSize;
    // Neither run holds an A5, so neither can start a sync word of its own.
    constexpr std::array<std::byte, kLeadingGarbage> kLeading{std::byte{0x11}, std::byte{0x22},
                                                              std::byte{0x33}};
    constexpr std::array<std::byte, kInnerGarbage> kInner{std::byte{0x66}, std::byte{0x77},
                                                          std::byte{0x88}};
    constexpr auto kFlipped = [] {
        auto f = kWorkedExample;
        f[tlm::kFixedHeaderSize] ^= std::byte{0xFF};
        return f;
    }();
    constexpr auto kUnknownId = [] {
        auto f = kWorkedExample;
        f[tlm::kMessageIdOffset] = std::byte{0x02};
        return WithValidCrc(f);
    }();

    constexpr auto kMixed = [&] {
        std::array<std::byte, kMixedSize> buffer{};
        std::size_t offset = 0;
        const auto append = [&](std::span<const std::byte> bytes) {
            std::copy(bytes.begin(), bytes.end(),
                      buffer.begin() + static_cast<std::ptrdiff_t>(offset));
            offset += bytes.size();
        };
        append(kLeading);
        append(kWorkedExample);
        append(kFlipped);
        append(kInner);
        append(std::span<const std::byte>{kFalseStartStream}.first(kFalseStartSize));
        append(kUnknownId);
        append(kWorkedExample);
        append(std::span<const std::byte>{kWorkedExample}.first(kPartialSize));
        return buffer;
    }();

    // Two good frames and the skipped unknown-id frame. The flipped frame and the false start are
    // not frames here: their bytes come back as Resync and Discard.
    const Pulled pulled = pullAll(kMixed, 3 * kWorkedExampleLength);
    ASSERT_EQ(pulled.events.size(), 8U);

    // The counts below are literals read off the layout, not computed from kResyncShift.
    const auto *const leading = std::get_if<tlm::Discard>(&pulled.events[0]);
    ASSERT_NE(leading, nullptr);
    EXPECT_EQ(leading->count, 3U);

    const auto *const first = std::get_if<tlm::DecodedFrame>(&pulled.events[1]);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->frame_header.sequence, std::uint16_t{40001});

    const auto *const flipped = std::get_if<tlm::Resync>(&pulled.events[2]);
    ASSERT_NE(flipped, nullptr);
    EXPECT_EQ(flipped->error.code, tlm::ErrorCode::BadChecksum);
    EXPECT_EQ(flipped->count, 2U);

    // The 16 bytes of the flipped frame after the shift, plus the 3 of garbage.
    const auto *const inner = std::get_if<tlm::Discard>(&pulled.events[3]);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->count, 19U);

    const auto *const false_start = std::get_if<tlm::Resync>(&pulled.events[4]);
    ASSERT_NE(false_start, nullptr);
    EXPECT_EQ(false_start->error.code, tlm::ErrorCode::BadChecksum);
    EXPECT_EQ(false_start->count, 2U);

    // The 12 zeros between the false start's sync word and the unknown-id frame's.
    const auto *const zeros = std::get_if<tlm::Discard>(&pulled.events[5]);
    ASSERT_NE(zeros, nullptr);
    EXPECT_EQ(zeros->count, 12U);

    const auto *const unknown = std::get_if<tlm::Error>(&pulled.events[6]);
    ASSERT_NE(unknown, nullptr);
    EXPECT_EQ(unknown->code, tlm::ErrorCode::UnknownMessageId);
    EXPECT_EQ(unknown->detail, std::uint8_t{0x02});

    const auto *const last = std::get_if<tlm::DecodedFrame>(&pulled.events[7]);
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->frame_header.sequence, std::uint16_t{40001});

    // The last value is also where the partial frame starts: the stream stops in front of it.
    const std::vector<std::size_t> expected_consumed{3, 21, 23, 42, 44, 56, 74, 92};
    EXPECT_EQ(pulled.consumed_after, expected_consumed);
    EXPECT_EQ(pulled.consumed_after.back(), kMixedSize - kPartialSize);
}
