#ifndef TLM_FRAMER_HPP
#define TLM_FRAMER_HPP

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

FrameResult frame(std::span<const std::byte> /*data*/) {
    return Incomplete{};
}

} // namespace tlm

#endif // TLM_FRAMER_HPP