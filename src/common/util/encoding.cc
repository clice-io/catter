#include "util/encoding.h"

#include <string>
#include <string_view>

#ifdef CATTER_WINDOWS
#include <windows.h>
#endif

namespace catter::util {

std::string to_utf8(const std::filesystem::path& path) {
    auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

std::filesystem::path from_utf8(std::string_view text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

#ifdef CATTER_WINDOWS

std::wstring wide_from_code_page(std::string_view text, unsigned code_page) {
    if(text.empty()) {
        return {};
    }
    const auto size = static_cast<int>(text.size());
    const int length = MultiByteToWideChar(code_page, 0, text.data(), size, nullptr, 0);
    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(code_page, 0, text.data(), size, wide.data(), length);
    return wide;
}

std::wstring to_wide(std::string_view text) {
    return wide_from_code_page(text, CP_UTF8);
}

std::string from_wide(std::wstring_view text) {
    if(text.empty()) {
        return {};
    }
    const auto size = static_cast<int>(text.size());
    const int length =
        WideCharToMultiByte(CP_UTF8, 0, text.data(), size, nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), size, utf8.data(), length, nullptr, nullptr);
    return utf8;
}

Utf8Arguments::Utf8Arguments(int argc, wchar_t* argv[]) {
    strings.reserve(static_cast<size_t>(argc));
    for(int i = 0; i < argc; ++i) {
        strings.push_back(from_wide(argv[i]));
    }
    for(auto& text: strings) {
        pointers.push_back(text.data());
    }
    pointers.push_back(nullptr);
}

#endif

}  // namespace catter::util
