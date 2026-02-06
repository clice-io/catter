#pragma once
#include <string_view>
#include <vector>
#include <ranges>
#include <string>

inline auto split2vec(std::string_view str) {
    return std::views::split(str, ' ') |
           std::views::transform([](auto&& rng) { return std::string(rng.begin(), rng.end()); }) |
           std::ranges::to<std::vector<std::string>>();
};
