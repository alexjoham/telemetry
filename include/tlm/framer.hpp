#ifndef TLM_FRAMER_HPP
#define TLM_FRAMER_HPP

#include "constants.hpp"
#include <cstddef>
#include <span>
#include <variant>

namespace tlm {

struct Found {
    std::size_t length;
};
struct Incomplete {};
struct Discard {
    std::size_t count;
};
using FrameResult = std::variant<Found, Incomplete, Discard>;

[[nodiscard]] constexpr FrameResult frame(std::span<const std::byte> data) noexcept {
    if (data.size() >= 2) {
        if (data[0] == kSyncByte0 && data[1] == kSyncByte1) {
            if (data.size() > kLengthFieldOffset) {
                const std::size_t length = std::to_integer<std::size_t>(data[kLengthFieldOffset]);
                const std::size_t frame_length = kFixedHeaderSize + length + kCrcSize;
                if (data.size() >= frame_length) {
                    return Found{frame_length};
                } else {
                    return Incomplete{};
                }
            }
            return Incomplete{};
        } else {
            for (std::size_t i = 0; i < data.size() - 1; i++) {
                if (data[i] == kSyncByte0 && data[i + 1] == kSyncByte1) {
                    return Discard{i};
                }
            }
            if (data[data.size() - 1] == kSyncByte0) {
                return Discard{data.size() - 1};
            }
        }
        return Discard{data.size()};
    } else if (data.size() == 1 && data[0] != kSyncByte0) {
        return Discard{1};
    }
    return Incomplete{};
}

} // namespace tlm

#endif // TLM_FRAMER_HPP