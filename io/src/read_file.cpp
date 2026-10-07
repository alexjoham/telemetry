#include "tlm/io/read_file.hpp"
#include "tlm/result.hpp"
#include <cstddef>
#include <filesystem>
#include <vector>

namespace tlm::io {

Result<std::vector<std::byte>, IoError> read_file(const std::filesystem::path & /*path*/) {
    return IoError::CannotOpen;
}

} // namespace tlm::io
