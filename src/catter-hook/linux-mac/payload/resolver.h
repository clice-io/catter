#pragma once

#include <climits>
#include <expected>
#include <filesystem>
#include <limits.h>
#include <string_view>

namespace catter {

/**
 * This class implements the logic how the program execution resolves the
 * executable path from the system environment.
 *
 * The resolution logic implemented as a class to be able to unit test
 * the code and avoid memory allocation.
 */
namespace resolver {
/**
 * Resolve the executable from system environments.
 *
 * @return resolved executable path as absolute path.
 */
[[nodiscard]]
std::expected<std::filesystem::path, int> from_current_directory(std::string_view file);

[[nodiscard]]
std::expected<std::filesystem::path, int> from_path(std::string_view file, const char** envp);

[[nodiscard]]
std::expected<std::filesystem::path, int> from_search_path(std::string_view file,
                                                           const char* search_path);

};  // namespace resolver
}  // namespace catter
