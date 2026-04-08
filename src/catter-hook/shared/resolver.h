#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#ifdef CATTER_WINDOWS
#include <type_traits>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#endif

namespace catter::hook::shared {

#ifdef CATTER_WINDOWS

template <typename char_t>
concept WindowsChar = std::is_same_v<char_t, char> || std::is_same_v<char_t, wchar_t>;

template <WindowsChar char_t>
struct WindowsSearchContext {
    std::basic_string<char_t> current_directory;
    std::basic_string<char_t> current_drive_root;
    std::basic_string<char_t> module_directory;
    std::basic_string<char_t> system_directory;
    std::basic_string<char_t> windows_directory;
    std::vector<std::basic_string<char_t>> path_entries;
};

template <WindowsChar char_t>
std::basic_string<char_t> extract_command_line_token(std::basic_string_view<char_t> command_line);

template <WindowsChar char_t>
class WindowsResolver {
public:
    explicit WindowsResolver(WindowsSearchContext<char_t> context);

    [[nodiscard]]
    static WindowsResolver from_current_process();

    [[nodiscard]]
    std::expected<std::basic_string<char_t>, DWORD>
        resolve_application_name(std::basic_string_view<char_t> application_name) const;

    [[nodiscard]]
    std::expected<std::basic_string<char_t>, DWORD>
        resolve_command_line_token(std::basic_string_view<char_t> executable_token) const;

private:
    [[nodiscard]]
    std::expected<std::basic_string<char_t>, DWORD>
        resolve_with_search_paths(std::basic_string_view<char_t> token,
                                  const std::vector<std::basic_string<char_t>>& search_paths) const;

    WindowsSearchContext<char_t> context_;
};

extern template class WindowsResolver<char>;
extern template class WindowsResolver<wchar_t>;

#else

[[nodiscard]]
std::expected<std::filesystem::path, int> resolve_path_like(std::string_view file);

[[nodiscard]]
std::expected<std::filesystem::path, int> resolve_from_environment(std::string_view file,
                                                                   const char** envp);

[[nodiscard]]
std::expected<std::filesystem::path, int> resolve_from_search_path(std::string_view file,
                                                                   std::string_view search_path);

#endif

}  // namespace catter::hook::shared
