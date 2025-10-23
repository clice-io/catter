#pragma once
#include <string>
#include <system_error>
#include <string_view>


#include <windows.h>

namespace catter {
static auto constexpr capture_root = "catter-captured";

static auto constexpr hook_dll = "catter-hook64.dll";

inline std::string wstring_to_utf8(const std::wstring& wstr, std::error_code& ec) {
    if (wstr.empty()) {
        return {};
    }

    const int utf8_size = ::WideCharToMultiByte(
        CP_UTF8,          // Target code page: UTF-8
        0,                // Flags
        wstr.c_str(),     // Source UTF-16 string
        static_cast<int>(wstr.length()), // Length of source string in wchar_t's
        NULL,             // Unused for size calculation
        0,                // Unused for size calculation
        NULL,             // Unused
        NULL              // Unused
    );

    if (utf8_size == 0){
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }

    std::string utf8_str(utf8_size, 0);

    const int result = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        static_cast<int>(wstr.length()),
        utf8_str.data(), 
        utf8_size,
        NULL,
        NULL
    );

    if (result == 0){
        ec = std::error_code(GetLastError(), std::system_category());
        return {};
    }

    return utf8_str;
}

}

namespace meta {
template <typename T>
consteval std::string_view type_name() {
    std::string_view name = 
        #if defined(__clang__) || defined(__GNUC__)
            __PRETTY_FUNCTION__;  // Clang / GCC
        #elif defined(_MSC_VER)
            __FUNCSIG__;         // MSVC
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
    constexpr std::string_view prefix = "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl meta::type_name<";
    constexpr std::string_view suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

}