#pragma once
#include <system_error>
#include <vector>
#include <string>
#include <filesystem>
#include "librpc/data.h"

namespace catter::util {
std::vector<std::string> get_environment() noexcept;

std::filesystem::path get_catter_root_path();

/**
 * @return the executable path of current process
 */
std::filesystem::path get_executable_path();

/**
 * @return the log path used by catter
 */
std::filesystem::path get_log_path();

}  // namespace catter::util
