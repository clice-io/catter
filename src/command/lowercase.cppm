module;

#include "modules/prelude.h"

export module catter.command:lowercase;

export namespace catter {

/// `text` with its ASCII letters lowered, as option and file names compare without case.
inline std::string lowercase(std::string_view text) {
    return text | std::views::transform([](char c) {
               return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
           }) |
           std::ranges::to<std::string>();
}

}  // namespace catter
