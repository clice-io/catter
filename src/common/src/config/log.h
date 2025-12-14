#pragma once

namespace catter::config::log {
/// like [2024-06-15 14:23:01.123] [INFO] [pid:12345][tid:123] message
#ifdef DEBUG
constexpr static char LOG_PATTERN_FILE[] =
    "[%@] [%Y-%m-%d %H:%M:%S.%e] [%l] [pid:%P][tid:%t] \n %v";
#else
constexpr static char LOG_PATTERN_FILE[] = "[%Y-%m-%d %H:%M:%S.%e] [%l] [pid:%P][tid:%t] \n %v";
#endif
constexpr static char LOG_PATTERN_CONSOLE[] = "%v";

constexpr static auto DAY_LIMITS = 7;
}  // namespace catter::config::log
