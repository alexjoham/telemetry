#include "tm/core.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace tm_core {

std::uint16_t Core::crc16(std::span<const std::byte> data) {
    std::uint16_t output = 0xFFFF;
    constexpr std::uint16_t pol = 0x1021;
    for (const std::byte b : data) {
        output = output ^ static_cast<std::uint16_t>(std::to_integer<std::uint16_t>(b) << 8);
        for (int i = 0; i < 8; i++) {
            if (output & 0x8000) {
                output = static_cast<std::uint16_t>(output << 1) ^ pol;
            } else {
                output = static_cast<std::uint16_t>(output << 1);
            }
        }
    }
    return output;
}

} // namespace tm_core