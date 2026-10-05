#ifndef TLM_STREAM_HPP
#define TLM_STREAM_HPP

#include "tlm/constants.hpp"
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

namespace detail {
template <typename... Ts> struct overloaded : Ts... {
    using Ts::operator()...;
};
template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;
} // namespace detail

class Stream {
  private:
    std::span<const std::byte> data_;
    std::size_t offset_ = 0;

  public:
    [[nodiscard]] constexpr std::optional<Event> next() noexcept {
        const std::span<const std::byte> rest = data_.subspan(offset_);
        return std::visit(
            detail::overloaded{
                [&](const Found &found) -> std::optional<Event> {
                    return decode(rest.first(found.length))
                        .match(
                            [&](const DecodedFrame &decoded) -> std::optional<Event> {
                                offset_ += found.length;
                                return decoded;
                            },
                            [&](const Error &error) -> std::optional<Event> {
                                switch (error.code) {
                                case ErrorCode::BadChecksum:
                                case ErrorCode::MalformedFrame:
                                    offset_ += kResyncShift;
                                    return Resync{error, kResyncShift};
                                case ErrorCode::UnsupportedVersion:
                                case ErrorCode::UnknownMessageId:
                                case ErrorCode::WrongPayloadLength:
                                    offset_ += found.length;
                                    return error;
                                }
                                // Only reachable with a value outside the named enumerators.
                                // Resync, because it does not trust the length byte.
                                offset_ += kResyncShift;
                                return Resync{error, kResyncShift};
                            });
                },
                [](const Incomplete &) -> std::optional<Event> { return std::nullopt; },
                [&](const Discard &discard) -> std::optional<Event> {
                    offset_ += discard.count;
                    return discard;
                }},
            frame(rest));
    }

    [[nodiscard]] constexpr std::size_t consumed() const noexcept {
        return offset_;
    }

    explicit constexpr Stream(std::span<const std::byte> data) noexcept : data_(data) {
    }
};

} // namespace tlm

#endif // TLM_STREAM_HPP
