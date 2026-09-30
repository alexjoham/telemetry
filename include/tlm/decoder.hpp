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

inline constexpr std::byte kSupportedMessageId{0x01};

struct DecodedFrame {
    FrameHeader frame_header;
    VehicleState vehicle_state;
};

[[nodiscard]] constexpr Result<DecodedFrame, Error>
decode(std::span<const std::byte> frame) noexcept {
    if (frame.size() < kFixedHeaderSize + kCrcSize) {
        return Error{ErrorCode::MalformedFrame};
    }

    const std::size_t total_length = frame.size();
    const std::uint16_t crc = crc16(frame.first(total_length - kCrcSize));
    if (!(frame[total_length - kCrcSize] == static_cast<std::byte>(crc & 0xFF) &&
          frame[total_length - 1] == static_cast<std::byte>(crc >> 8))) {
        return Error{ErrorCode::BadChecksum};
    }

    const std::size_t stated_length = std::to_integer<std::size_t>(frame[kLengthFieldOffset]);
    if (kFixedHeaderSize + stated_length + kCrcSize != frame.size()) {
        return Error{ErrorCode::MalformedFrame};
    }

    const std::byte version = frame[kVersionOffset];
    if (version != kSupportedVersion) {
        return Error{ErrorCode::UnsupportedVersion, std::to_integer<std::uint8_t>(version)};
    }

    const std::byte message_id = frame[kMessageIdOffset];
    if (message_id != kSupportedMessageId) {
        return Error{ErrorCode::UnknownMessageId, std::to_integer<std::uint8_t>(message_id)};
    }
    return DecodedFrame{};
}

} // namespace tlm

#endif // TLM_DECODER_HPP