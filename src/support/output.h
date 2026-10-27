#pragma once
#include <format>
#include <print>
#include <string>
#include <utility>

namespace catter::output {

constexpr std::format_string<std::string> BLUE = "\033[34m{}\033[0m";
constexpr std::format_string<std::string> GREEN = "\033[32m{}\033[0m";
constexpr std::format_string<std::string> YELLOW = "\033[33m{}\033[0m";
constexpr std::format_string<std::string> RED = "\033[31m{}\033[0m";

/// Prints a warning to the user, on a line of its own, in yellow.
template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    std::println(YELLOW, "Warning: " + std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace catter::output
