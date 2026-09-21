#ifndef TLM_CONSTANTS_HPP
#define TLM_CONSTANTS_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

namespace tlm {

inline constexpr std::byte kSyncByte0{0xA5};
inline constexpr std::byte kSyncByte1{0xC3};

inline constexpr std::size_t kLengthFieldOffset = 10;

inline constexpr std::size_t kFixedHeaderSize = 12;
inline constexpr std::size_t kCrcSize = 2;

inline constexpr std::size_t kMaxPayloadSize = std::numeric_limits<std::uint8_t>::max();
inline constexpr std::size_t kMaxFrameSize = kFixedHeaderSize + kMaxPayloadSize + kCrcSize;

} // namespace tlm

#endif // TLM_CONSTANTS_HPP
