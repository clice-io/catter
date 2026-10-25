#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

/// @file encoding.h
/// @brief catter's strings hold UTF-8 on every platform. Paths, and on Windows
/// the system's own strings, are converted where they meet one: Windows reads a
/// narrow string in the ANSI code page, which holds few characters past ASCII,
/// so every call into it takes UTF-16 instead.

namespace catter {

/// The UTF-8 spelling of `path`.
std::string to_utf8(const std::filesystem::path& path);

/// The path a UTF-8 string names.
std::filesystem::path from_utf8(std::string_view text);

#ifdef CATTER_WINDOWS

/// UTF-8 as the UTF-16 Windows takes; an invalid byte becomes U+FFFD.
std::wstring to_wide(std::string_view text);

/// UTF-16 from Windows as UTF-8; an unpaired surrogate, which a Windows name
/// may hold, becomes U+FFFD.
std::string from_wide(std::wstring_view text);

/// Text in a code page, such as the ANSI code page a program's narrow strings
/// are in, as UTF-16.
std::wstring wide_from_code_page(std::string_view text, unsigned code_page);

/// The arguments wmain is given, as the UTF-8 argv main would take: the CRT's
/// argv for main is in the ANSI code page.
class Utf8Arguments {
public:
    Utf8Arguments(int argc, wchar_t* argv[]);

    char** argv() noexcept {
        return pointers.data();
    }

private:
    std::vector<std::string> strings;
    std::vector<char*> pointers;
};

#endif

}  // namespace catter
