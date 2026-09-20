#ifndef TLM_CRC_HPP
#define TLM_CRC_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace tlm {

[[nodiscard]] constexpr std::uint16_t crc16(std::span<const std::byte> data) noexcept {
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

} // namespace tlm

#endif // TLM_CRC_HPP
