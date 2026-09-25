#ifndef ERROR_CODE_HPP
#define ERROR_CODE_HPP

#include <cstdint>

enum class ErrorCode : std::uint8_t {
    MalformedFrame,
    BadChecksum,
    UnsupportedVersion,
    UnknownMessageId,
    WrongPayloadLength,
};

struct Error {
    ErrorCode code;
    std::uint8_t detail;
};

#endif // ERROR_CODE_HPP