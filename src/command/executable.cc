#include "command/executable.h"

#include <format>
#include <regex>
#include <string_view>

namespace catter {

std::regex executable_pattern(std::string_view name, std::string_view suffix) {
    const auto pattern = std::format(R"(^(?:.*[\\/])?(?:{}){}(?:\.exe)?$)", name, suffix);
#ifdef CATTER_WINDOWS
    constexpr auto flags = std::regex::ECMAScript | std::regex::icase;
#else
    constexpr auto flags = std::regex::ECMAScript;
#endif
    return std::regex(pattern, flags);
}

}  // namespace catter
