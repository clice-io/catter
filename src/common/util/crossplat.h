#pragma once
#include <chrono>
#include <filesystem>
#include <ranges>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

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

template <typename Range, typename T>
    requires std::ranges::range<std::decay_t<Range>> &&
             std::is_same_v<T, std::ranges::range_value_t<Range>>
void append_range_to_vector(std::vector<T>& buffer, Range&& range) {
#ifdef __cpp_lib_containers_ranges
    buffer.append_range(std::forward<Range>(range));
#else
    buffer.insert(buffer.end(), std::ranges::begin(range), std::ranges::end(range));
#endif
}

template <typename... Args,
          typename T = std::common_type_t<std::ranges::range_value_t<std::decay_t<Args>>...>>
    requires (std::ranges::range<std::decay_t<Args>> && ...)
std::vector<T> merge_range_to_vector(Args&&... args) {
    std::vector<T> buffer;
    buffer.reserve((std::ranges::size(args) + ...));
    (append_range_to_vector(buffer, std::forward<Args>(args)), ...);
    return buffer;
}

}  // namespace catter::util
