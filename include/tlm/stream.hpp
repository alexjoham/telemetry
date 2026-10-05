#ifndef TLM_STREAM_HPP
#define TLM_STREAM_HPP

#include "tlm/decoder.hpp"
#include "tlm/error.hpp"
#include "tlm/framer.hpp"
#include <cstddef>
#include <optional>
#include <span>
#include <variant>

namespace tlm {

struct Resync {
    Error error;
    std::size_t count;
};

using Event = std::variant<DecodedFrame, Error, Discard, Resync>;

class Stream {
  private:
    std::span<const std::byte> data_;
    std::size_t offset_;

  public:
    [[nodiscard]] constexpr std::optional<Event> next() noexcept {
        return std::nullopt;
    }

    [[nodiscard]] constexpr std::size_t consumed() const noexcept {
        return 0;
    }

    explicit Stream(std::span<const std::byte> data) : data_(data) {
        offset_ = 0;
    }
};

} // namespace tlm

#endif // TLM_STREAM_HPP
