#include "test_frames.hpp"
#include "tlm/constants.hpp"
#include "tlm/decoder.hpp"
#include "tlm/error.hpp"
#include "tlm/result.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <span>

namespace {

// Restamping the normative frame reproduces it byte for byte.
static_assert(WithValidCrc(kWorkedExample) == kWorkedExample);

// The worked example's decoded fields.
void ExpectWorkedExampleFields(const tlm::DecodedFrame &frame) {
    EXPECT_EQ(frame.frame_header.version, std::uint8_t{1});
    EXPECT_EQ(frame.frame_header.message_id, std::uint8_t{1});
    EXPECT_EQ(frame.frame_header.sequence, std::uint16_t{40001});
    EXPECT_EQ(frame.frame_header.timestamp, std::uint32_t{439041101});
    EXPECT_EQ(frame.vehicle_state.actuator_flags, std::uint8_t{0b1011});
    EXPECT_EQ(frame.vehicle_state.drive_mode, tlm::DriveMode::Autonomous);
    EXPECT_EQ(frame.vehicle_state.steering_count, std::uint16_t{1443});
    EXPECT_EQ(frame.vehicle_state.brake_count, std::uint16_t{3231});
}

// Constant evaluation rejects undefined behaviour, so decoding the worked example here checks it
// on every compiler and build type rather than only under the sanitiser jobs.
constexpr auto kDecodedWorkedExample = tlm::decode(kWorkedExample);
static_assert(kDecodedWorkedExample.ok() != nullptr);

} // namespace

TEST(DecoderTest, WorkedExampleDecodesToSpecValues) {
    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kWorkedExample);

    const tlm::DecodedFrame *frame = result.ok();
    ASSERT_NE(frame, nullptr);
    ExpectWorkedExampleFields(*frame);
}

// Gate 1: one byte short of the smallest legal frame.
TEST(DecoderTest, ShortSpanReturnsMalformedFrame) {
    constexpr std::size_t kTooShort = tlm::kFixedHeaderSize + tlm::kCrcSize - 1;
    const tlm::Result<tlm::DecodedFrame, tlm::Error> result =
        tlm::decode(std::span<const std::byte>{kWorkedExample}.first(kTooShort));

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::MalformedFrame);
}

// Gate 1: nothing to read at all.
TEST(DecoderTest, EmptySpanReturnsMalformedFrame) {
    const tlm::Result<tlm::DecodedFrame, tlm::Error> result =
        tlm::decode(std::span<const std::byte>{});

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::MalformedFrame);
}

// Gate 2: one payload byte flipped, CRC left alone.
TEST(DecoderTest, CorruptPayloadIsBadChecksum) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kFixedHeaderSize] ^= std::byte{0x01};
        return f;
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::BadChecksum);
}

// The sync word is under the CRC, so a frame read from a false start fails here.
TEST(DecoderTest, CorruptSyncWordIsBadChecksum) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[0] ^= std::byte{0x01};
        return f;
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::BadChecksum);
}

// Gate 2 before gate 4: the version byte is untrusted until the CRC has passed.
TEST(DecoderTest, UnsupportedVersionUnderBadCrcIsBadChecksum) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kVersionOffset] = std::byte{0x02};
        return f;
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::BadChecksum);
}

// Gate 2 before gate 3: the length field stays untrusted content until the CRC covering it has
// passed, so the same disagreement without a restamp is a bad checksum.
TEST(DecoderTest, LengthFieldDisagreeingUnderBadCrcIsBadChecksum) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kLengthFieldOffset] = std::byte{0x00};
        return f;
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::BadChecksum);
}

// Gate 3: the length field claims a 0-byte payload, the span carries 4.
TEST(DecoderTest, LengthFieldDisagreeingWithSpanIsMalformed) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kLengthFieldOffset] = std::byte{0x00};
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::MalformedFrame);
}

// Coverage comes from the span's size; slicing it from the length field reads 214 bytes out of 18.
TEST(DecoderTest, CrcCoverageComesFromSpanSizeNotLengthField) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kLengthFieldOffset] = std::byte{200};
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::MalformedFrame);
}

// Gate 4. The error carries the version byte.
TEST(DecoderTest, UnsupportedVersionIsRejected) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kVersionOffset] = std::byte{0x02};
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::UnsupportedVersion);
    EXPECT_EQ(error->detail, std::uint8_t{0x02});
}

// Gate 5. The error carries the id.
TEST(DecoderTest, UnknownMessageIdIsRejected) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kMessageIdOffset] = std::byte{0x02};
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::UnknownMessageId);
    EXPECT_EQ(error->detail, std::uint8_t{0x02});
}

// 0006's first ordering frame: the id table is version-dependent, so the version comes first.
TEST(DecoderTest, UnsupportedVersionOutranksUnknownMessageId) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kVersionOffset] = std::byte{0x02};
        f[tlm::kMessageIdOffset] = std::byte{0x99};
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::UnsupportedVersion);
    EXPECT_EQ(error->detail, std::uint8_t{0x02});
}

// 0006's second ordering frame: an unknown id has no expected length, so the id comes first.
// The span is 21 bytes so the length field stays consistent and gate 3 passes.
TEST(DecoderTest, UnknownMessageIdOutranksWrongPayloadLength) {
    constexpr std::size_t kPayloadSize = 7;
    constexpr auto kFrame = [] {
        std::array<std::byte, tlm::kFixedHeaderSize + kPayloadSize + tlm::kCrcSize> f{};
        std::copy_n(kWorkedExample.begin(), tlm::kFixedHeaderSize, f.begin());
        f[tlm::kMessageIdOffset] = std::byte{0x02};
        f[tlm::kLengthFieldOffset] = static_cast<std::byte>(kPayloadSize);
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::UnknownMessageId);
    EXPECT_EQ(error->detail, std::uint8_t{0x02});
}

// Gate 6: the smallest legal frame, 12 + 0 + 2. Id 0x01 defines a 4-byte payload, so 0 is wrong.
TEST(DecoderTest, EmptyPayloadForVehicleStateIsWrongPayloadLength) {
    constexpr auto kFrame = [] {
        std::array<std::byte, tlm::kFixedHeaderSize + tlm::kCrcSize> f{};
        std::copy_n(kWorkedExample.begin(), tlm::kFixedHeaderSize, f.begin());
        f[tlm::kLengthFieldOffset] = std::byte{0x00};
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::WrongPayloadLength);
    EXPECT_EQ(error->detail, std::uint8_t{0x01});
}

// Gate 6 from above, where a < comparison would let the frame through.
TEST(DecoderTest, OverlongPayloadForVehicleStateIsWrongPayloadLength) {
    constexpr std::size_t kPayloadSize = tlm::kVehicleStatePayloadSize + 1;
    constexpr auto kFrame = [] {
        std::array<std::byte, tlm::kFixedHeaderSize + kPayloadSize + tlm::kCrcSize> f{};
        std::copy_n(kWorkedExample.begin(), tlm::kFixedHeaderSize, f.begin());
        f[tlm::kLengthFieldOffset] = static_cast<std::byte>(kPayloadSize);
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::Error *error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->code, tlm::ErrorCode::WrongPayloadLength);
    EXPECT_EQ(error->detail, std::uint8_t{0x01});
}

// The reserved byte is for a future version to use; rejecting on it would defeat that.
TEST(DecoderTest, ReservedHeaderByteIsIgnored) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kReservedOffset] = std::byte{0xFF};
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::DecodedFrame *frame = result.ok();
    ASSERT_NE(frame, nullptr);
    ExpectWorkedExampleFields(*frame);
}

// Payload bits 30-31 are reserved: the top two bits of the last payload byte.
TEST(DecoderTest, ReservedPayloadBitsAreIgnored) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        f[tlm::kFixedHeaderSize + tlm::kVehicleStatePayloadSize - 1] |= std::byte{0xC0};
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::DecodedFrame *frame = result.ok();
    ASSERT_NE(frame, nullptr);
    ExpectWorkedExampleFields(*frame);
}

// The two modes no other fixture reaches. Bits 4-5 sit between the actuator flags and the
// steering angle, so both are asserted too and a mask off by one bit takes one of them with it.
TEST(DecoderTest, ManualAndAssistedDriveModesDecode) {
    constexpr auto kManual = [] {
        auto f = kWorkedExample;
        f[tlm::kFixedHeaderSize] &= std::byte{0xCF};
        return WithValidCrc(f);
    }();
    constexpr auto kAssisted = [] {
        auto f = kWorkedExample;
        f[tlm::kFixedHeaderSize] = (f[tlm::kFixedHeaderSize] & std::byte{0xCF}) | std::byte{0x10};
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> manual = tlm::decode(kManual);
    const tlm::DecodedFrame *manual_frame = manual.ok();
    ASSERT_NE(manual_frame, nullptr);
    EXPECT_EQ(manual_frame->vehicle_state.drive_mode, tlm::DriveMode::Manual);
    EXPECT_EQ(manual_frame->vehicle_state.actuator_flags, std::uint8_t{0b1011});
    EXPECT_EQ(manual_frame->vehicle_state.steering_count, std::uint16_t{1443});

    const tlm::Result<tlm::DecodedFrame, tlm::Error> assisted = tlm::decode(kAssisted);
    const tlm::DecodedFrame *assisted_frame = assisted.ok();
    ASSERT_NE(assisted_frame, nullptr);
    EXPECT_EQ(assisted_frame->vehicle_state.drive_mode, tlm::DriveMode::Assisted);
    EXPECT_EQ(assisted_frame->vehicle_state.actuator_flags, std::uint8_t{0b1011});
    EXPECT_EQ(assisted_frame->vehicle_state.steering_count, std::uint16_t{1443});
}

// All ones: every field at full scale, and the one DriveMode the worked example misses.
TEST(DecoderTest, FullScalePayloadDecodesToMaxima) {
    constexpr auto kFrame = [] {
        auto f = kWorkedExample;
        for (std::size_t i = tlm::kFixedHeaderSize;
             i < tlm::kFixedHeaderSize + tlm::kVehicleStatePayloadSize; ++i) {
            f[i] = std::byte{0xFF};
        }
        return WithValidCrc(f);
    }();

    const tlm::Result<tlm::DecodedFrame, tlm::Error> result = tlm::decode(kFrame);

    const tlm::DecodedFrame *frame = result.ok();
    ASSERT_NE(frame, nullptr);
    EXPECT_EQ(frame->vehicle_state.actuator_flags, std::uint8_t{0b1111});
    EXPECT_EQ(frame->vehicle_state.drive_mode, tlm::DriveMode::Fault);
    EXPECT_EQ(frame->vehicle_state.steering_count, std::uint16_t{4095});
    EXPECT_EQ(frame->vehicle_state.brake_count, std::uint16_t{4095});
}
