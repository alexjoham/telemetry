#include "tlm/constants.hpp"
#include <cstddef>
#include <gtest/gtest.h>
#include <vector>

namespace {

// Volatile so the index never becomes a compile-time constant. A constant one lets GCC report
// -Warray-bounds at -O2, and -Werror would then fail the Release build instead of the sanitiser.
volatile std::size_t g_index_past_the_end = tlm::kFixedHeaderSize + tlm::kCrcSize;

} // namespace

// Reads one byte past the allocation through a raw pointer, not .at(), and feeds it to an
// assertion so the read survives dead-code elimination.
TEST(SanitizerProofTest, ReadPastEndOfHeapBuffer) {
    const std::vector<std::byte> frame(tlm::kFixedHeaderSize + tlm::kCrcSize);
    const std::byte *const bytes = frame.data();
    const std::size_t index = g_index_past_the_end;

    EXPECT_EQ(bytes[index], std::byte{0x00});
}
