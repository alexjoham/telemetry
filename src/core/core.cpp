#include "tm/core.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace tm_core {

std::uint16_t Core::crc16(std::span<const std::byte> /*data*/) {
    return 0;
}

} // namespace tm_core