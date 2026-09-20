#ifndef TLM_CONSTANTS_HPP
#define TLM_CONSTANTS_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

namespace tlm {

constexpr std::byte kSyncByte0{0xA5};
constexpr std::byte kSyncByte1{0xC3};

constexpr std::size_t kLengthFieldOffset = 10;

constexpr std::size_t kFixedHeaderSize = 12;
constexpr std::size_t kCrcSize = 2;

constexpr std::size_t kMaxPayloadSize = std::numeric_limits<std::uint8_t>::max();
constexpr std::size_t kMaxFrameSize = kFixedHeaderSize + kMaxPayloadSize + kCrcSize;

} // namespace tlm

#endif // TLM_CONSTANTS_HPP
