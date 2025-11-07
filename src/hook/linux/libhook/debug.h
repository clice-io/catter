#pragma once
#ifdef DEBUG

#include <print>
#include <cstdlib>
#include <format>
#include <print>
#include <source_location>
#include <string_view>

namespace catter::debug {

constexpr const inline char RESET[] = "\033[0m";
constexpr const inline char BOLD[] = "\033[1m";
constexpr const inline char UNDERLINE[] = "\033[4m";
constexpr const inline char REVERSED[] = "\033[7m";
constexpr const inline char BLACK[] = "\033[30m";
constexpr const inline char RED[] = "\033[31m";
constexpr const inline char GREEN[] = "\033[32m";
constexpr const inline char YELLOW[] = "\033[33m";
constexpr const inline char BLUE[] = "\033[34m";
constexpr const inline char MAGENTA[] = "\033[35m";
constexpr const inline char CYAN[] = "\033[36m";
constexpr const inline char WHITE[] = "\033[37m";

struct StyledText {
    std::string_view content;
    std::string_view color;
};

template <typename T>
    requires std::convertible_to<T, std::string_view>
constexpr StyledText style(T content, const std::string_view color) {
    return StyledText{.content = std::string_view(content), .color = color};
}

template <typename... Args>
constexpr void colorPrintln(const std::string_view color,
                            std::format_string<Args...> fmt,
                            Args&&... args) {
    std::println("{}", style(std::format(fmt, std::forward<Args>(args)...), color));
}

template <typename... Args>
constexpr void colorPrint(const std::string_view color,
                          std::format_string<Args...> fmt,
                          Args&&... args) {
    std::print("{}", style(std::format(fmt, std::forward<Args>(args)...), color));
}

#define SPECIFIY_COLOR_PRINT(FnName, ColorName)                                                    \
    template <typename... Args>                                                                    \
    constexpr void FnName##Ln(std::format_string<Args...> fmt, Args&&... args) {                   \
        colorPrintln(ColorName, fmt, std::forward<Args>(args)...);                                 \
    };                                                                                             \
    template <typename... Args>                                                                    \
    constexpr void FnName(std::format_string<Args...> fmt, Args&&... args) {                       \
        colorPrint(ColorName, fmt, std::forward<Args>(args)...);                                   \
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

template <typename... Args>
constexpr void info(std::source_location location,
                    std ::format_string<Args...> fmt,
                    Args&&... args) {
#if LOG_LEVEL <= 1
    std::println("[INFO]{} {}",
                 location,
                 style(std::format(fmt, std::forward<Args>(args)...), GREEN));
#endif
};

template <typename... Args>
constexpr void warn(std::source_location location,
                    std ::format_string<Args...> fmt,
                    Args&&... args) {
#if LOG_LEVEL <= 2
    std::println("[WARN]{} {}",
                 location,
                 style(std::format(fmt, std::forward<Args>(args)...), YELLOW));
#endif
};

template <typename... Args>
constexpr void error(std::source_location location,
                     std ::format_string<Args...> fmt,
                     Args&&... args) {
#if LOG_LEVEL <= 3
    std::println("[ERROR]{} {}",
                 location,
                 style(std::format(fmt, std::forward<Args>(args)...), RED));
#endif
};

template <typename... Args>
constexpr void panic(std::source_location location,
                     std ::format_string<Args...> fmt,
                     Args&&... args) {
#if LOG_LEVEL <= 3
    std::println("[ERROR]{} {}",
                 location,
                 style(std::format(fmt, std::forward<Args>(args)...), RED));
    std::abort();
#endif
};

}  // namespace catter::debug

template <>
struct std::formatter<catter::debug::StyledText> {
    constexpr static auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const catter::debug::StyledText& s, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}{}{}", s.color, s.content, catter::debug::RESET);
    }
};

// location
inline auto loc(std::source_location location = std::source_location::current()) {
    return location;
}

template <>
struct std::formatter<std::source_location> {
    constexpr static auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const std::source_location& loc, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "[{}:{}:{}]", loc.file_name(), loc.line(), loc.column());
    }
};

#define INFO(...) catter::debug::info(std::source_location::current(), __VA_ARGS__)
#define WARN(...) catter::debug::warn(std::source_location::current(), __VA_ARGS__)
#define ERROR(...) catter::debug::error(std::source_location::current(), __VA_ARGS__)
#define PANIC(...) catter::debug::panic(std::source_location::current(), __VA_ARGS__)

#endif

#ifndef DEBUG
#define INFO(...)
#define WARN(...)
#define ERROR(...)
#define PANIC(...)
#endif  // !DEBUG
