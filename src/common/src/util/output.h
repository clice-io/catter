#pragma once
#include <print>
#include <cstdlib>
#include <format>
#include <string_view>

namespace catter::output {

constexpr char RESET[] = "\033[0m";
constexpr char BOLD[] = "\033[1m";
constexpr char UNDERLINE[] = "\033[4m";
constexpr char REVERSED[] = "\033[7m";
constexpr char BLACK[] = "\033[30m";
constexpr char RED[] = "\033[31m";
constexpr char GREEN[] = "\033[32m";
constexpr char YELLOW[] = "\033[33m";
constexpr char BLUE[] = "\033[34m";
constexpr char MAGENTA[] = "\033[35m";
constexpr char CYAN[] = "\033[36m";
constexpr char WHITE[] = "\033[37m";

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

}  // namespace catter::output

template <>
struct std::formatter<catter::output::StyledText> {
    constexpr static auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const catter::output::StyledText& s, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}{}{}", s.color, s.content, catter::output::RESET);
    }
};
