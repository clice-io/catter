#pragma once
#include <string_view>
#include <vector>
#include <ranges>
#include <string>

inline auto split2vec(std::string_view str) {
    auto views = std::views::split(str, ' ') |
           std::views::transform([](auto&& rng) { return std::string(rng.begin(), rng.end()); });
    return std::vector<std::string>(views.begin(), views.end());
};
