#ifndef TESTS_TEST_FRAMES_HPP
#define TESTS_TEST_FRAMES_HPP

#include <array>
#include <cstddef>

// The normative frame from docs/format.md, all 18 bytes: L = 12 + 4 + 2. Shared so the CRC and
// framer tests cannot drift; the CRC test takes .first(16), since a CRC cannot cover its own field.
inline constexpr std::array<std::byte, 18> kWorkedExample{
    std::byte{0xA5}, std::byte{0xC3}, std::byte{0x01}, std::byte{0x01}, std::byte{0x41},
    std::byte{0x9C}, std::byte{0x4D}, std::byte{0x3C}, std::byte{0x2B}, std::byte{0x1A},
    std::byte{0x04}, std::byte{0x00}, std::byte{0xEB}, std::byte{0x68}, std::byte{0x7D},
    std::byte{0x32}, std::byte{0x85}, std::byte{0x4A}};

inline constexpr std::size_t kWorkedExampleLength = kWorkedExample.size();

#endif // TESTS_TEST_FRAMES_HPP
