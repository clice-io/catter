#pragma once
#include <cstddef>
#include <cstdlib>
#include <format>
#include <print>
#include <string>

namespace catter::output {

#define RESET "{}\033[0m"

constexpr std::format_string<std::string> BOLD = "\033[1m" RESET;
constexpr std::format_string<std::string> UNDERLINE = "\033[4m" RESET;
constexpr std::format_string<std::string> REVERSED = "\033[7m" RESET;
constexpr std::format_string<std::string> BLACK = "\033[30m" RESET;
constexpr std::format_string<std::string> RED = "\033[31m" RESET;
constexpr std::format_string<std::string> GREEN = "\033[32m" RESET;
constexpr std::format_string<std::string> YELLOW = "\033[33m" RESET;
constexpr std::format_string<std::string> BLUE = "\033[34m" RESET;
constexpr std::format_string<std::string> MAGENTA = "\033[35m" RESET;
constexpr std::format_string<std::string> CYAN = "\033[36m" RESET;
constexpr std::format_string<std::string> WHITE = "\033[37m" RESET;

#define SPECIFIY_COLOR_PRINT(FnName, Color)                                                        \
    template <typename... Args>                                                                    \
    constexpr void FnName##ln(std::format_string<Args...> fmt, Args&&... args) {                   \
        std::println(Color, std::format(fmt, std::forward<Args>(args)...));                        \
    };                                                                                             \
    template <typename... Args>                                                                    \
    constexpr void FnName(std::format_string<Args...> fmt, Args&&... args) {                       \
        std::print(Color, std::format(fmt, std::forward<Args>(args)...));                          \
    };

SPECIFIY_COLOR_PRINT(bold, BOLD)
SPECIFIY_COLOR_PRINT(underline, UNDERLINE)
SPECIFIY_COLOR_PRINT(reversed, REVERSED)
SPECIFIY_COLOR_PRINT(black, BLACK)
SPECIFIY_COLOR_PRINT(red, RED)
SPECIFIY_COLOR_PRINT(green, GREEN)
SPECIFIY_COLOR_PRINT(yellow, YELLOW)
SPECIFIY_COLOR_PRINT(blue, BLUE)
SPECIFIY_COLOR_PRINT(magenta, MAGENTA)
SPECIFIY_COLOR_PRINT(cyan, CYAN)
SPECIFIY_COLOR_PRINT(white, WHITE)

#undef SPECIFIY_COLOR_PRINT
#undef RESET

}  // namespace catter::output
