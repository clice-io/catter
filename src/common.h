#pragma once
#include <string_view>

namespace meta {
template <typename T>
consteval std::string_view type_name() {
    std::string_view name =
#if defined(__clang__) || defined(__GNUC__)
        __PRETTY_FUNCTION__;  // Clang / GCC
#elif defined(_MSC_VER)
        __FUNCSIG__;  // MSVC
#else
        static_assert(false, "Unsupported compiler");
#endif

#if defined(__clang__)
    constexpr std::string_view prefix = "std::string_view meta::type_name() [T = ";
    constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
    constexpr std::string_view prefix = "consteval std::string_view meta::type_name() [with T = ";
    constexpr std::string_view suffix = "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
    constexpr std::string_view prefix =
        "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl meta::type_name<";
    constexpr std::string_view suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

}  // namespace meta
