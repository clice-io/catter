#pragma once
#include <system_error>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <chrono>

#include "librpc/data.h"

namespace catter::util {
std::vector<std::string> get_environment() noexcept;

std::filesystem::path get_catter_root_path();

/**
 * @return the executable path of current process
 */
std::filesystem::path get_executable_path();

/**
 * @return the data path used by catter
 */
std::filesystem::path get_catter_data_path();

inline uint64_t unique_id() {
    auto id = std::this_thread::get_id();
    auto id_hash = std::hash<std::thread::id>{}(id);
    auto time = std::chrono::system_clock::now().time_since_epoch().count();
    return static_cast<uint64_t>(id_hash) ^ static_cast<uint64_t>(time);
}

}  // namespace catter::util
