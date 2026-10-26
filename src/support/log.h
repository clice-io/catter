#pragma once
#include <ranges>
#include <string>
#include <string_view>

#include "spdlog/spdlog.h"

/// Logging through spdlog, wrapped in macros.

#ifdef DEBUG
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#endif  // DEBUG

#ifndef DEBUG
#define LOG_TRACE(...) (void)0
#define LOG_DEBUG(...) (void)0
#endif

#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)

namespace catter::log {
/// With `reopen` the log file is opened for each message rather than kept
/// open: the hook logs inside the build's programs, which may put another
/// file at a descriptor kept open, as a child does between fork and exec.
void init_logger(const std::string& logger_name,
                 std::string_view file_path,
                 bool cmdline = false,
                 bool reopen = false) noexcept;

void mute_logger() noexcept;

template <typename Range>
    requires std::ranges::range<std::decay_t<Range>> &&
             std::is_same_v<char, std::ranges::range_value_t<Range>>
inline std::string to_hex(const Range& range, size_t max_bytes = 0) {
    std::string hex_str;
    size_t count = max_bytes > 0 ? (std::ranges::min)(max_bytes, std::ranges::size(range))
                                 : std::ranges::size(range);
    for(const auto& byte: std::ranges::views::take(range, count)) {
        hex_str += std::format("{:02x} ", static_cast<unsigned char>(byte));
    }
    hex_str.pop_back();  // Remove the trailing space
    return hex_str;
}

}  // namespace catter::log
