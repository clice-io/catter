#pragma once
#include <ranges>
#include <filesystem>
#include <iostream>
#include "spdlog/spdlog.h"

/**
 * @file spdlog.h
 * @brief Provide spdlog logging functionality wrapped with macro
 */

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
void init_logger(const std::string& logger_name,
                 const std::filesystem::path& file_path,
                 bool cmdline = false) noexcept;

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

inline std::string escape(const std::string& input) {
    std::ostringstream result;
    for(char c: input) {
        switch(c) {
            case '\\': result << "\\\\"; break;
            case '"': result << "\\\""; break;
            case '\'': result << "\\'"; break;
            case '\n': result << "\\n"; break;
            case '\r': result << "\\r"; break;
            case '\t': result << "\\t"; break;
            case '\b': result << "\\b"; break;
            case '\f': result << "\\f"; break;
            case '\v': result << "\\v"; break;
            case '\0': result << "\\0"; break;
            default:
                if(static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) {
                    result << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                           << static_cast<int>(static_cast<unsigned char>(c));
                } else {
                    result << c;
                }
                break;
        }
    }
    return result.str();
}
}  // namespace catter::log
