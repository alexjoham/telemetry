#ifndef TLM_CRC_HPP
#define TLM_CRC_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace tlm {

std::uint16_t crc16(std::span<const std::byte> data);

} // namespace tlm

#endif // TLM_CRC_HPP
