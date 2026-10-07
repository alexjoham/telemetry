#include "test_frames.hpp"
#include "tlm/decoder.hpp"
#include "tlm/io/read_file.hpp"
#include "tlm/stream.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <ios>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace {

[[nodiscard]] std::filesystem::path writeTempFile(std::string_view name,
                                                  std::span<const std::byte> bytes) {
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream out{path, std::ios::binary};
    out.write(reinterpret_cast<const char *>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return path;
}

} // namespace

TEST(IoTest, ReadFileReturnsTheExactBytesWritten) {
    constexpr std::array<std::byte, 6> kBytes{std::byte{0x00}, std::byte{0x0A}, std::byte{0x0D},
                                              std::byte{0x0A}, std::byte{0x1A}, std::byte{0xFF}};
    const std::filesystem::path path = writeTempFile("tlm_io_test_exact_bytes.bin", kBytes);

    const auto result = tlm::io::read_file(path);
    std::filesystem::remove(path);

    const std::vector<std::byte> *const read = result.ok();
    ASSERT_NE(read, nullptr);
    EXPECT_EQ(*read, std::vector<std::byte>(kBytes.begin(), kBytes.end()));
}

TEST(IoTest, ReadFileOfMissingPathReturnsCannotOpen) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tlm_io_test_does_not_exist.bin";
    std::filesystem::remove(path);

    const auto result = tlm::io::read_file(path);

    const tlm::io::IoError *const error = result.err();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(*error, tlm::io::IoError::CannotOpen);
}

TEST(IoTest, WorkedExampleFromFileYieldsOneDecodedFrame) {
    const std::filesystem::path path =
        writeTempFile("tlm_io_test_worked_example.bin", kWorkedExample);

    const auto result = tlm::io::read_file(path);
    std::filesystem::remove(path);
    const std::vector<std::byte> *const bytes = result.ok();
    ASSERT_NE(bytes, nullptr);

    tlm::Stream stream{*bytes};
    const std::optional<tlm::Event> event = stream.next();
    ASSERT_TRUE(event.has_value());
    const auto *const decoded = std::get_if<tlm::DecodedFrame>(&*event);
    ASSERT_NE(decoded, nullptr);
    EXPECT_EQ(decoded->frame_header.sequence, std::uint16_t{40001});
    EXPECT_FALSE(stream.next().has_value());
}
