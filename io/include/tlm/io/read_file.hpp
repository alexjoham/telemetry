#ifndef TLM_IO_READ_FILE
#define TLM_IO_READ_FILE

#include "tlm/result.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace tlm::io {

enum class IoError : std::uint8_t { CannotOpen, ReadFailed };

[[nodiscard]] Result<std::vector<std::byte>, IoError> read_file(const std::filesystem::path &path);

} // namespace tlm::io

#endif // TLM_IO_READ_FILE
