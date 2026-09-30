#ifndef TLM_DECODER_HPP
#define TLM_DECODER_HPP

#include "constants.hpp"
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

struct DecodedFrame {
    FrameHeader frame_header;
    VehicleState vehicle_state;
};

[[nodiscard]] constexpr Result<DecodedFrame, Error>
decode(std::span<const std::byte> frame) noexcept {
    if (frame.size() < kFixedHeaderSize + kCrcSize) {
        return Error{ErrorCode::MalformedFrame};
    }
    return DecodedFrame{};
}

} // namespace tlm

#endif // TLM_DECODER_HPP