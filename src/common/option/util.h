#pragma once

#include <span>
#include <string_view>

namespace catter::opt {
constexpr inline std::string_view _pfx_dash[] = {"-"};
constexpr inline std::string_view _pfx_dash_double[] = {"-", "--"};
constexpr inline std::string_view _pfx_double[] = {"--"};
constexpr inline std::string_view _pfx_all[] = {"--", "/", "-"};
constexpr inline std::string_view _pfx_slash_dash[] = {"/", "-"};

constexpr inline auto pfx_none = std::span<const std::string_view>();
constexpr inline auto pfx_dash = std::span<const std::string_view>(_pfx_dash);
constexpr inline auto pfx_dash_double = std::span<const std::string_view>(_pfx_dash_double);
constexpr inline auto pfx_double = std::span<const std::string_view>(_pfx_double);
constexpr inline auto pfx_all = std::span<const std::string_view>(_pfx_all);
constexpr inline auto pfx_slash_dash = std::span<const std::string_view>(_pfx_slash_dash);
}  // namespace catter::opt
