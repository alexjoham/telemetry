#ifndef TLM_DECODER_HPP
#define TLM_DECODER_HPP

#include "constants.hpp"
#include "crc.hpp"
#include "error.hpp"
#include "result.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace tlm {

struct FrameHeader {
    std::uint8_t version;
    std::uint8_t message_id;
    std::uint16_t sequence;
    std::uint32_t timestamp;
};

static_assert(sizeof(FrameHeader) == 8);
static_assert(alignof(FrameHeader) == 4);

enum class DriveMode : std::uint8_t {
    Manual = 0,
    Assisted = 1,
    Autonomous = 2,
    Fault = 3,
};

struct VehicleState {
    std::uint8_t actuator_flags;
    DriveMode drive_mode;
    std::uint16_t steering_count;
    std::uint16_t brake_count;
};

static_assert(sizeof(VehicleState) == 6);
static_assert(alignof(VehicleState) == 2);

// The payload length message id 0x01 defines. Message-specific, so it does not live with the
// framing constants.
inline constexpr std::size_t kVehicleStatePayloadSize = 4;

inline constexpr std::byte kVehicleStateMessageId{0x01};

struct DecodedFrame {
    FrameHeader frame_header;
    VehicleState vehicle_state;
};

namespace detail {

// The wire's byte order is stated here once rather than at each multi-byte field.
template <typename T>
[[nodiscard]] constexpr T read_le(std::span<const std::byte> frame, std::size_t offset) noexcept {
    T value{};
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        value = static_cast<T>(value | (std::to_integer<T>(frame[offset + i]) << (8 * i)));
    }
    return value;
}

} // namespace detail

[[nodiscard]] constexpr Result<DecodedFrame, Error>
decode(std::span<const std::byte> frame) noexcept {
    const std::size_t total_length = frame.size();
    if (total_length < kFixedHeaderSize + kCrcSize) {
        return Error{ErrorCode::MalformedFrame};
    }

    const std::size_t crc_offset = total_length - kCrcSize;
    if (crc16(frame.first(crc_offset)) != detail::read_le<std::uint16_t>(frame, crc_offset)) {
        return Error{ErrorCode::BadChecksum};
    }

    const std::size_t stated_length = std::to_integer<std::size_t>(frame[kLengthFieldOffset]);
    if (kFixedHeaderSize + stated_length + kCrcSize != total_length) {
        return Error{ErrorCode::MalformedFrame};
    }

    const std::byte version = frame[kVersionOffset];
    if (version != kSupportedVersion) {
        return Error{ErrorCode::UnsupportedVersion, std::to_integer<std::uint8_t>(version)};
    }

    const std::byte message_id = frame[kMessageIdOffset];
    if (message_id != kVehicleStateMessageId) {
        return Error{ErrorCode::UnknownMessageId, std::to_integer<std::uint8_t>(message_id)};
    }

    if (stated_length != kVehicleStatePayloadSize) {
        return Error{ErrorCode::WrongPayloadLength, std::to_integer<std::uint8_t>(message_id)};
    }

    const FrameHeader frame_header{
        .version = std::to_integer<std::uint8_t>(version),
        .message_id = std::to_integer<std::uint8_t>(message_id),
        .sequence = detail::read_le<std::uint16_t>(frame, kSequenceNumberOffset),
        .timestamp = detail::read_le<std::uint32_t>(frame, kTimestampOffset),
    };

    // Bit-packed across all four bytes, not byte-aligned. See format.md's VehicleState table.
    const std::uint32_t payload = detail::read_le<std::uint32_t>(frame, kFixedHeaderSize);
    const VehicleState vehicle_state{
        .actuator_flags = static_cast<std::uint8_t>(payload & 0xF),
        // Two bits, and DriveMode defines all four values, so the cast is total.
        .drive_mode = static_cast<DriveMode>((payload >> 4) & 0x3),
        .steering_count = static_cast<std::uint16_t>((payload >> 6) & 0xFFF),
        .brake_count = static_cast<std::uint16_t>((payload >> 18) & 0xFFF),
    };

    return DecodedFrame{frame_header, vehicle_state};
}

} // namespace tlm

#endif // TLM_DECODER_HPP