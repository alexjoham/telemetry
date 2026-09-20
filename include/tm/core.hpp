#ifndef TM_CORE_HPP
#define TM_CORE_HPP

#include <cstddef>
#include <cstdint>
#include <span>

namespace tm_core {

class Core {
  public:
    std::uint16_t crc16(std::span<const std::byte> data);
};

} // namespace tm_core

#endif // TM_CORE_HPP
