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

class Stream {
  private:
    std::span<const std::byte> data_;
    std::size_t offset_ = 0;

  public:
    [[nodiscard]] constexpr std::optional<Event> next() noexcept {
        const std::span<const std::byte> rest = data_.subspan(offset_);
        FrameResult framed = frame(rest);
        if (const auto *const found = std::get_if<Found>(&framed)) {
            const auto decoded = decode(rest.first(found->length));
            const auto *result = decoded.ok();
            if (result != nullptr) {
                offset_ += found->length;
                return *result;
            }
            const auto *error = decoded.err();
            if (error != nullptr) {
                switch (error->code) {
                case ErrorCode::BadChecksum:
                case ErrorCode::MalformedFrame:
                    offset_ += kResyncShift;
                    return Resync{*error, kResyncShift};
                case ErrorCode::UnsupportedVersion:
                case ErrorCode::UnknownMessageId:
                case ErrorCode::WrongPayloadLength:
                    offset_ += found->length;
                    break;
                }
                return *error;
            }
        } else if (std::get_if<Incomplete>(&framed)) {
            return std::nullopt;
        } else if (const auto *const discard = std::get_if<Discard>(&framed)) {
            offset_ += discard->count;
            return *discard;
        }
        return std::nullopt;
    }

    [[nodiscard]] constexpr std::size_t consumed() const noexcept {
        return offset_;
    }

    explicit constexpr Stream(std::span<const std::byte> data) : data_(data) {
    }
};

} // namespace tlm

#endif // TLM_STREAM_HPP
