#include "tlm/constants.hpp"
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <vector>

namespace {

// Volatile so the index never becomes a compile-time constant. A constant one lets GCC report
// -Warray-bounds at -O2, and -Werror would then fail the Release build instead of the sanitiser.
volatile std::size_t g_index_past_the_end = tlm::kFixedHeaderSize + tlm::kCrcSize;

// Volatile for the same reason: a constant operand is diagnosed as -Woverflow at compile time.
volatile std::int32_t g_frames_seen = std::numeric_limits<std::int32_t>::max();

} // namespace

// Reads one byte past the allocation through a raw pointer, not .at(), and feeds it to an
// assertion so the read survives dead-code elimination.
TEST(SanitizerProofTest, ReadPastEndOfHeapBuffer) {
    const std::vector<std::byte> frame(tlm::kFixedHeaderSize + tlm::kCrcSize);
    const std::byte *const bytes = frame.data();
    const std::size_t index = g_index_past_the_end;

    EXPECT_EQ(bytes[index], std::byte{0x00});
}

// Overflows a signed counter: undefined, but not a memory error. EXPECT_NE holds both when the
// addition wraps at -O0 and when -O2 assumes it cannot overflow, so only the sanitiser reacts.
TEST(SanitizerProofTest, SignedOverflowOnFrameCounter) {
    const std::int32_t frames_seen = g_frames_seen;

    const std::int32_t next = frames_seen + 1;

    EXPECT_NE(next, frames_seen);
}
