#include "test_frames.hpp"
#include "tlm/constants.hpp"
#include "tlm/crc.hpp"
#include "tlm/framer.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include <span>
#include <variant>

namespace {

// kResyncShift is 0001's rule. kFrameLength and kSingleByte are the alternatives it rejected: a
// test runs two of them on one stream and compares. kNothing drops zero, to trip the progress guard.
enum class RejectionPolicy { kResyncShift, kFrameLength, kSingleByte, kNothing };

[[nodiscard]] std::size_t dropOnRejection(RejectionPolicy policy, std::size_t length) {
    switch (policy) {
    case RejectionPolicy::kResyncShift:
        return tlm::kResyncShift;
    case RejectionPolicy::kFrameLength:
        return length;
    case RejectionPolicy::kSingleByte:
        return 1;
    case RejectionPolicy::kNothing:
        return 0;
    }
    return 0;
}

// A one-byte drop recovers exactly what the rule recovers, so recovered cannot tell them apart.
// calls is what can: it counts frame() invocations, the work 0001 rejects the one-byte drop for.
struct DrainResult {
    std::size_t recovered;
    std::size_t calls;
};

constexpr std::size_t kNoiseSize = 18;
constexpr std::size_t kFrameCount = 15;
constexpr std::size_t kStreamSize = kNoiseSize + (kFrameCount * kWorkedExampleLength);

// Drives frame(), checks each frame's CRC, and returns how many passed. This is the caller loop
// from 0001's framer and decoder tables. It stays in the test until tm_io decides who owns it.
[[nodiscard]] DrainResult drain(std::span<const std::byte> buffer, RejectionPolicy policy) {
    // Every iteration drops at least one byte, so the loop cannot legally run more times than the
    // buffer has bytes. Exceeding that means it is stuck, and failing beats spinning.
    const std::size_t max_iterations = buffer.size();
    std::size_t iterations = 0;
    std::size_t recovered = 0;
    std::size_t calls = 0;

    while (!buffer.empty()) {
        if (iterations++ == max_iterations) {
            ADD_FAILURE() << "drain exceeded " << max_iterations << " iterations";
            break;
        }

        const std::size_t size_before = buffer.size();
        const tlm::FrameResult result = tlm::frame(buffer);
        calls++;

        if (const auto *const found = std::get_if<tlm::Found>(&result)) {
            const std::span<const std::byte> candidate = buffer.first(found->length);
            const std::uint16_t computed =
                tlm::crc16(candidate.first(found->length - tlm::kCrcSize));

            const std::span<const std::byte> crc_field = candidate.last(tlm::kCrcSize);
            const std::uint16_t stored = static_cast<std::uint16_t>(
                std::to_integer<std::uint16_t>(crc_field[0]) |
                static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(crc_field[1]) << 8));

            if (computed == stored) {
                recovered++;
                buffer = buffer.subspan(found->length);
            } else {
                buffer = buffer.subspan(dropOnRejection(policy, found->length));
            }
        } else if (const auto *const discard = std::get_if<tlm::Discard>(&result)) {
            buffer = buffer.subspan(discard->count);
        } else {
            // Incomplete tells a real caller to wait, but the fixture has no more data coming.
            break;
        }

        // Also per iteration, so a stuck loop names where it stuck, not just that it ran too long.
        if (buffer.size() >= size_before) {
            ADD_FAILURE() << "iteration consumed nothing, " << size_before << " bytes left";
            break;
        }
    }
    return DrainResult{recovered, calls};
}

// The noise claims length 200, so the framer computes L = 214 and only the CRC rejects it. Its
// other bytes are zero, so the noise holds no accidental sync word of its own.
[[nodiscard]] std::array<std::byte, kStreamSize> makeStream() {
    std::array<std::byte, kStreamSize> stream{};
    stream[0] = tlm::kSyncByte0;
    stream[1] = tlm::kSyncByte1;
    stream[tlm::kLengthFieldOffset] = std::byte{200};
    for (std::size_t i = 0; i < kFrameCount; i++) {
        std::copy(kWorkedExample.begin(), kWorkedExample.end(),
                  stream.begin() +
                      static_cast<std::ptrdiff_t>(kNoiseSize + (i * kWorkedExampleLength)));
    }
    return stream;
}

constexpr std::size_t kPaddingSize = 60;

// Where the second sync word sits: A5 C3 has no proper prefix that is also a suffix, so the
// earliest a match can begin again is two bytes in. Not kResyncShift, which is the value on test.
constexpr std::size_t kAdjacentOffset = 2;

constexpr std::size_t kAdjacentStreamSize = kAdjacentOffset + kWorkedExampleLength + kPaddingSize;

// The false start has no header, so byte 10 falls inside the real frame: 0x2B, giving L = 57.
constexpr std::size_t kBogusFrameLength =
    tlm::kFixedHeaderSize +
    std::to_integer<std::size_t>(kWorkedExample[tlm::kLengthFieldOffset - kAdjacentOffset]) +
    tlm::kCrcSize;

// Below L the framer never answers Found, so every policy recovers 0 whatever the shift does.
static_assert(kAdjacentStreamSize >= kBogusFrameLength);

[[nodiscard]] std::array<std::byte, kAdjacentStreamSize> makeAdjacentStream() {
    std::array<std::byte, kAdjacentStreamSize> stream{};
    stream[0] = tlm::kSyncByte0;
    stream[1] = tlm::kSyncByte1;
    std::copy(kWorkedExample.begin(), kWorkedExample.end(),
              stream.begin() + static_cast<std::ptrdiff_t>(kAdjacentOffset));
    return stream;
}

} // namespace

// Both policies run over one stream. Recovering 15 is no evidence on its own, since a one-byte
// drop would manage it too; the second count is what tells the rule from a lucky one.
TEST(RecoveryTest, ResyncShiftRecoversEveryFrameBehindAFalseStart) {
    const std::array<std::byte, kStreamSize> stream = makeStream();

    EXPECT_EQ(drain(stream, RejectionPolicy::kResyncShift).recovered, kFrameCount);

    // 12 + 200 + 2 = 214 dropped, 288 - 214 = 74 left, 74 / 18 = 4 frames with 2 bytes over.
    EXPECT_EQ(drain(stream, RejectionPolicy::kFrameLength).recovered, std::size_t{4});
}

// The shift must not overshoot: a frame starting kResyncShift bytes in must still be found. In
// test one any drop up to 18 recovers all 15, so it cannot catch an overshoot.
TEST(RecoveryTest, ShiftDoesNotSkipAnAdjacentSyncWord) {
    const std::array<std::byte, kAdjacentStreamSize> stream = makeAdjacentStream();

    EXPECT_EQ(drain(stream, RejectionPolicy::kResyncShift).recovered, std::size_t{1});
    EXPECT_EQ(drain(stream, RejectionPolicy::kFrameLength).recovered, std::size_t{0});
}

// The shift must not undershoot either, which no recovered count can see: dropping one still
// recovers the frame, one wasted call later. So assert the landing, not the total.
TEST(RecoveryTest, ShiftLandsOnTheAdjacentSyncWord) {
    const std::array<std::byte, kAdjacentStreamSize> stream = makeAdjacentStream();
    const std::span<const std::byte> after_drop =
        std::span<const std::byte>{stream}.subspan(tlm::kResyncShift);

    const tlm::FrameResult result = tlm::frame(after_drop);

    // A Discard here means the drop stopped short and the framer had to scan the rest of the way.
    const auto *const found = std::get_if<tlm::Found>(&result);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->length, kWorkedExampleLength);
}

// 0001 rejects the one-byte drop on cost, not correctness: it recovers the same frames and spends
// a call and a scan per rejection learning what the sync word already said. Here 3 calls against 4.
TEST(RecoveryTest, ShiftCostsFewerFramerCallsThanASingleByteDrop) {
    const std::array<std::byte, kAdjacentStreamSize> stream = makeAdjacentStream();

    const DrainResult rule = drain(stream, RejectionPolicy::kResyncShift);
    const DrainResult single_byte = drain(stream, RejectionPolicy::kSingleByte);

    // Equal recovery is the premise: without it the two are not comparable on cost.
    EXPECT_EQ(rule.recovered, single_byte.recovered);
    EXPECT_LT(rule.calls, single_byte.calls);
}

// The other tests never trip the guard, so a broken one would still pass them. kNothing drops
// zero, standing in for a future kResyncShift of 0.
TEST(RecoveryTest, ZeroDropTripsTheForwardProgressGuard) {
    const std::array<std::byte, kStreamSize> stream = makeStream();

    EXPECT_NONFATAL_FAILURE(static_cast<void>(drain(stream, RejectionPolicy::kNothing)),
                            "consumed nothing");
}
