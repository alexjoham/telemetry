#ifndef TLM_CONSTANTS_HPP
#define TLM_CONSTANTS_HPP

#include <cstddef>

namespace tlm {

// Derived from the layout in docs/format.md: 12-byte header + 255 max payload
// (1-byte length field) + 2-byte CRC. Not an independent limit.
constexpr std::size_t kMaxFrameSize = 269;

} // namespace tlm

#endif // TLM_CONSTANTS_HPP
