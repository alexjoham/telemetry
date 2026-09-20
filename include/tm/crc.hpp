#ifndef TLM_HPP
#define TLM_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace tlm {

    std::uint16_t crc16(std::span<const std::byte> data);

} // namespace tlm

#endif // TLM_HPP
