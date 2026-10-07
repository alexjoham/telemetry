#include "tlm/io/read_file.hpp"
#include "tlm/result.hpp"
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <system_error>
#include <vector>

namespace tlm::io {

Result<std::vector<std::byte>, IoError> read_file(const std::filesystem::path &path) {
    std::error_code ec;
    const bool regular = std::filesystem::is_regular_file(path, ec);
    if (ec) {
        return IoError::CannotOpen;
    }
    if (!regular) {
        return IoError::NotARegularFile;
    }
    if (std::ifstream in{path, std::ios::binary | std::ios::ate}) {
        const auto size = in.tellg();
        if (size == -1) {
            return IoError::ReadFailed;
        }
        std::vector<std::byte> input(static_cast<std::size_t>(size));
        in.seekg(0);
        if (!in.read(reinterpret_cast<char *>(input.data()), size)) {
            return IoError::ReadFailed;
        }
        return input;
    }
    return IoError::CannotOpen;
}

} // namespace tlm::io
