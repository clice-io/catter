#include "shared/resolver.h"

#ifdef CATTER_WINDOWS

#include <algorithm>
#include <filesystem>
#include <limits>

namespace catter::hook::shared {
namespace {

template <WindowsChar char_t>
struct WinApiBufferDecision {
    bool done = false;
    size_t output_size = 0;
    size_t next_size = 0;
};

template <WindowsChar char_t, typename size_t_winapi, typename api_call_t, typename decision_t>
std::basic_string<char_t> call_winapi_with_growing_buffer(size_t initial_size,
                                                          api_call_t&& call,
                                                          decision_t&& decide) {
    constexpr auto max_size = static_cast<size_t>((std::numeric_limits<size_t_winapi>::max)());
    size_t buffer_size = initial_size < 1 ? 1 : initial_size;
    if(buffer_size > max_size) {
        return {};
    }

    std::basic_string<char_t> buffer(buffer_size, char_t('\0'));
    while(true) {
        auto result = call(buffer.data(), static_cast<size_t_winapi>(buffer.size()));
        if(result == 0) {
            return {};
        }

        auto decision = decide(result, buffer.size());
        if(decision.done) {
            if(decision.output_size > buffer.size()) {
                return {};
            }
            buffer.resize(decision.output_size);
            return buffer;
        }

        auto next_size =
            decision.next_size > (buffer.size() + 1) ? decision.next_size : (buffer.size() + 1);
        if(next_size > max_size) {
            return {};
        }
        buffer.resize(next_size);
    }
}

template <WindowsChar char_t>
DWORD fix_get_environment_variable(const char_t* name, char_t* buffer, DWORD size) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetEnvironmentVariableA(name, buffer, size);
    } else {
        return GetEnvironmentVariableW(name, buffer, size);
    }
}

template <WindowsChar char_t>
DWORD fix_get_current_directory(DWORD size, char_t* buffer) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetCurrentDirectoryA(size, buffer);
    } else {
        return GetCurrentDirectoryW(size, buffer);
    }
}

template <WindowsChar char_t>
DWORD fix_get_module_file_name(HMODULE module, char_t* buffer, DWORD size) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetModuleFileNameA(module, buffer, size);
    } else {
        return GetModuleFileNameW(module, buffer, size);
    }
}

template <WindowsChar char_t>
DWORD fix_search_path(const char_t* path,
                      const char_t* file_name,
                      const char_t* extension,
                      DWORD buffer_size,
                      char_t* buffer,
                      char_t** file_part) {
    if constexpr(std::is_same_v<char_t, char>) {
        return SearchPathA(path, file_name, extension, buffer_size, buffer, file_part);
    } else {
        return SearchPathW(path, file_name, extension, buffer_size, buffer, file_part);
    }
}

template <WindowsChar char_t>
UINT fix_get_system_directory(char_t* buffer, UINT size) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetSystemDirectoryA(buffer, size);
    } else {
        return GetSystemDirectoryW(buffer, size);
    }
}

template <WindowsChar char_t>
UINT fix_get_windows_directory(char_t* buffer, UINT size) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetWindowsDirectoryA(buffer, size);
    } else {
        return GetWindowsDirectoryW(buffer, size);
    }
}

template <WindowsChar char_t>
std::basic_string<char_t> get_environment_variable_dynamic(const char_t* name,
                                                           size_t initial_size = 256) {
    return call_winapi_with_growing_buffer<char_t, DWORD>(
        initial_size,
        [&](char_t* buffer, DWORD size) {
            return fix_get_environment_variable<char_t>(name, buffer, size);
        },
        [](DWORD result, size_t buffer_size) -> WinApiBufferDecision<char_t> {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision<char_t>{.done = true,
                                                    .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision<char_t>{.next_size = static_cast<size_t>(result)};
        });
}

template <WindowsChar char_t>
std::basic_string<char_t> get_current_directory_dynamic(size_t initial_size = MAX_PATH) {
    return call_winapi_with_growing_buffer<char_t, DWORD>(
        initial_size,
        [&](char_t* buffer, DWORD size) { return fix_get_current_directory<char_t>(size, buffer); },
        [](DWORD result, size_t buffer_size) -> WinApiBufferDecision<char_t> {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision<char_t>{.done = true,
                                                    .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision<char_t>{.next_size = static_cast<size_t>(result) + 1};
        });
}

template <WindowsChar char_t>
std::basic_string<char_t> get_module_directory(HMODULE module, size_t initial_size = MAX_PATH) {
    auto module_path = call_winapi_with_growing_buffer<char_t, DWORD>(
        initial_size,
        [&](char_t* buffer, DWORD size) {
            return fix_get_module_file_name<char_t>(module, buffer, size);
        },
        [](DWORD result, size_t buffer_size) -> WinApiBufferDecision<char_t> {
            auto written = static_cast<size_t>(result);
            if(written < buffer_size) {
                return WinApiBufferDecision<char_t>{.done = true, .output_size = written};
            }
            return WinApiBufferDecision<char_t>{.next_size = buffer_size * 2};
        });
    if(module_path.empty()) {
        return {};
    }
    if constexpr(std::is_same_v<char_t, char>) {
        return std::filesystem::path(module_path).parent_path().string();
    } else {
        return std::filesystem::path(module_path).parent_path().wstring();
    }
}

template <WindowsChar char_t>
std::basic_string<char_t> get_system_directory_dynamic(size_t initial_size = MAX_PATH) {
    return call_winapi_with_growing_buffer<char_t, UINT>(
        initial_size,
        [&](char_t* buffer, UINT size) { return fix_get_system_directory<char_t>(buffer, size); },
        [](UINT result, size_t buffer_size) -> WinApiBufferDecision<char_t> {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision<char_t>{.done = true,
                                                    .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision<char_t>{.next_size = static_cast<size_t>(result) + 1};
        });
}

template <WindowsChar char_t>
std::basic_string<char_t> get_windows_directory_dynamic(size_t initial_size = MAX_PATH) {
    return call_winapi_with_growing_buffer<char_t, UINT>(
        initial_size,
        [&](char_t* buffer, UINT size) { return fix_get_windows_directory<char_t>(buffer, size); },
        [](UINT result, size_t buffer_size) -> WinApiBufferDecision<char_t> {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision<char_t>{.done = true,
                                                    .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision<char_t>{.next_size = static_cast<size_t>(result) + 1};
        });
}

template <WindowsChar char_t>
std::vector<std::basic_string<char_t>> split_path_var(std::basic_string_view<char_t> value) {
    std::vector<std::basic_string<char_t>> segments;
    size_t start = 0;
    while(start <= value.size()) {
        auto split_pos = value.find(char_t(';'), start);
        auto token =
            value.substr(start,
                         split_pos == std::basic_string_view<char_t>::npos ? value.size() - start
                                                                           : split_pos - start);
        if(!token.empty()) {
            segments.emplace_back(token);
        }
        if(split_pos == std::basic_string_view<char_t>::npos) {
            break;
        }
        start = split_pos + 1;
    }
    return segments;
}

template <WindowsChar char_t>
void push_if_non_empty(std::vector<std::basic_string<char_t>>& out,
                       std::basic_string<char_t> value) {
    if(!value.empty()) {
        out.push_back(std::move(value));
    }
}

template <WindowsChar char_t>
std::basic_string<char_t> join_search_paths(
    const std::vector<std::basic_string<char_t>>& search_paths) {
    std::basic_string<char_t> merged;
    for(const auto& path: search_paths) {
        if(path.empty()) {
            continue;
        }
        if(!merged.empty()) {
            merged.push_back(char_t(';'));
        }
        merged.append(path);
    }
    return merged;
}

template <WindowsChar char_t>
std::expected<std::basic_string<char_t>, DWORD>
    search_path_for_token(std::basic_string_view<char_t> token,
                          const std::vector<std::basic_string<char_t>>& search_paths) {
    if(token.empty()) {
        return std::unexpected(ERROR_INVALID_PARAMETER);
    }

    auto merged_paths = join_search_paths(search_paths);
    auto token_str = std::basic_string<char_t>(token);
    constexpr auto exe_suffix = std::is_same_v<char_t, char> ? ".exe" : L".exe";
    std::basic_string<char_t> resolved =
        call_winapi_with_growing_buffer<char_t, DWORD>(MAX_PATH,
                                                       [&](char_t* buffer, DWORD size) {
                                                           return fix_search_path<char_t>(
                                                               merged_paths.empty()
                                                                   ? nullptr
                                                                   : merged_paths.c_str(),
                                                               token_str.c_str(),
                                                               exe_suffix,
                                                               size,
                                                               buffer,
                                                               nullptr);
                                                       },
                                                       [](DWORD result,
                                                          size_t buffer_size)
                                                           -> WinApiBufferDecision<char_t> {
                                                           if(static_cast<size_t>(result) <
                                                              buffer_size) {
                                                               return WinApiBufferDecision<char_t>{
                                                                   .done = true,
                                                                   .output_size =
                                                                       static_cast<size_t>(result)};
                                                           }
                                                           return WinApiBufferDecision<char_t>{
                                                               .next_size =
                                                                   static_cast<size_t>(result) +
                                                                   1};
                                                       });
    if(!resolved.empty()) {
        return resolved;
    }

    auto error = GetLastError();
    return std::unexpected(error == ERROR_SUCCESS ? ERROR_FILE_NOT_FOUND : error);
}

template <WindowsChar char_t>
std::basic_string<char_t> current_drive_root(std::basic_string_view<char_t> current_directory) {
    if(current_directory.size() < 2 || current_directory[1] != char_t(':')) {
        return {};
    }

    std::basic_string<char_t> root;
    root.push_back(current_directory[0]);
    root.push_back(char_t(':'));
    root.push_back(char_t('\\'));
    return root;
}

template <WindowsChar char_t>
constexpr std::basic_string_view<char_t> path_env_name = {};
template <>
constexpr std::basic_string_view<char> path_env_name<char> = "PATH";
template <>
constexpr std::basic_string_view<wchar_t> path_env_name<wchar_t> = L"PATH";

template <WindowsChar char_t>
constexpr std::basic_string_view<char_t> system16_name = {};
template <>
constexpr std::basic_string_view<char> system16_name<char> = "System";
template <>
constexpr std::basic_string_view<wchar_t> system16_name<wchar_t> = L"System";

template <WindowsChar char_t>
std::basic_string<char_t> append_child_directory(std::basic_string<char_t> base,
                                                 std::basic_string_view<char_t> name) {
    if(base.empty()) {
        return {};
    }
    if(base.back() != char_t('\\') && base.back() != char_t('/')) {
        base.push_back(char_t('\\'));
    }
    base.append(name);
    return base;
}

}  // namespace

template <WindowsChar char_t>
std::basic_string<char_t> extract_command_line_token(std::basic_string_view<char_t> command_line) {
    constexpr char_t quote_char = char_t('"');
    constexpr char_t space_char = char_t(' ');

    auto first_non_space =
        std::find_if_not(command_line.begin(),
                         command_line.end(),
                         [](char_t ch) { return ch == space_char; });
    std::basic_string_view<char_t> view(first_non_space, command_line.end());
    if(view.empty()) {
        return {};
    }
    if(view.front() == quote_char) {
        auto closing_quote = view.find(quote_char, 1);
        if(closing_quote == std::basic_string_view<char_t>::npos) {
            return std::basic_string<char_t>(view.substr(1));
        }
        return std::basic_string<char_t>(view.substr(1, closing_quote - 1));
    }

    auto first_space =
        std::find_if(view.begin(), view.end(), [](char_t ch) { return ch == space_char; });
    return std::basic_string<char_t>(view.begin(), first_space);
}

template <WindowsChar char_t>
WindowsResolver<char_t>::WindowsResolver(WindowsSearchContext<char_t> context) :
    context_(std::move(context)) {}

template <WindowsChar char_t>
WindowsResolver<char_t> WindowsResolver<char_t>::from_current_process() {
    auto current_directory = get_current_directory_dynamic<char_t>();
    auto windows_directory = get_windows_directory_dynamic<char_t>();
    return WindowsResolver<char_t>({
        .current_directory = current_directory,
        .current_drive_root = current_drive_root<char_t>(current_directory),
        .module_directory = get_module_directory<char_t>(nullptr),
        .system_directory = get_system_directory_dynamic<char_t>(),
        .windows_directory = windows_directory,
        .path_entries =
            split_path_var<char_t>(get_environment_variable_dynamic<char_t>(path_env_name<char_t>.data(),
                                                                            4096)),
    });
}

template <WindowsChar char_t>
std::expected<std::basic_string<char_t>, DWORD> WindowsResolver<char_t>::resolve_application_name(
    std::basic_string_view<char_t> application_name) const {
    std::vector<std::basic_string<char_t>> search_paths;
    search_paths.reserve(2);
    push_if_non_empty(search_paths, context_.current_drive_root);
    push_if_non_empty(search_paths, context_.current_directory);
    return resolve_with_search_paths(application_name, search_paths);
}

template <WindowsChar char_t>
std::expected<std::basic_string<char_t>, DWORD> WindowsResolver<char_t>::resolve_command_line_token(
    std::basic_string_view<char_t> executable_token) const {
    std::vector<std::basic_string<char_t>> search_paths;
    search_paths.reserve(5 + context_.path_entries.size());
    push_if_non_empty(search_paths, context_.module_directory);
    push_if_non_empty(search_paths, context_.current_directory);
    push_if_non_empty(search_paths, context_.system_directory);
    push_if_non_empty(
        search_paths,
        append_child_directory(context_.windows_directory, system16_name<char_t>));
    push_if_non_empty(search_paths, context_.windows_directory);
    for(const auto& path_entry: context_.path_entries) {
        push_if_non_empty(search_paths, path_entry);
    }
    return resolve_with_search_paths(executable_token, search_paths);
}

template <WindowsChar char_t>
std::expected<std::basic_string<char_t>, DWORD> WindowsResolver<char_t>::resolve_with_search_paths(
    std::basic_string_view<char_t> token,
    const std::vector<std::basic_string<char_t>>& search_paths) const {
    return search_path_for_token<char_t>(token, search_paths);
}

template class WindowsResolver<char>;
template class WindowsResolver<wchar_t>;

template std::basic_string<char> extract_command_line_token(std::basic_string_view<char>);
template std::basic_string<wchar_t> extract_command_line_token(std::basic_string_view<wchar_t>);

}  // namespace catter::hook::shared

#endif
