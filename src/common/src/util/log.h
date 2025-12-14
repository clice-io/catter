#pragma once
#include <filesystem>
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
}  // namespace catter::log
