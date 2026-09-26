#ifndef TLM_ERROR_HPP
#define TLM_ERROR_HPP

#include <cstdint>

namespace tlm {

    enum class ErrorCode : std::uint8_t {
        MalformedFrame,
        BadChecksum,
        UnsupportedVersion,
        UnknownMessageId,
        WrongPayloadLength,
    };

    struct Error {
        ErrorCode code;
        std::uint8_t detail = 0;
    };
} // namespace tlm

#endif // TLM_ERROR_HPP
