#ifdef CATTER_WINDOWS

#include "shared/resolver.h"

#include <algorithm>
#include <concepts>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

namespace catter::hook::shared::resolver {
namespace {

template <typename CharT>
constexpr CharT k_path_delimiter = CharT(';');

template <typename CharT>
constexpr CharT k_path_sep = CharT('\\');

template <typename CharT>
constexpr std::basic_string_view<CharT> k_exe_suffix = {};

template <>
constexpr std::basic_string_view<char> k_exe_suffix<char> = ".exe";

template <>
constexpr std::basic_string_view<wchar_t> k_exe_suffix<wchar_t> = L".exe";

template <typename CharT>
DWORD get_environment_variable(const CharT* name, CharT* buffer, DWORD size);

template <>
DWORD get_environment_variable<char>(const char* name, char* buffer, DWORD size) {
    return ::GetEnvironmentVariableA(name, buffer, size);
}

template <>
DWORD get_environment_variable<wchar_t>(const wchar_t* name, wchar_t* buffer, DWORD size) {
    return ::GetEnvironmentVariableW(name, buffer, size);
}

template <typename CharT>
DWORD get_current_directory(DWORD size, CharT* buffer);

template <>
DWORD get_current_directory<char>(DWORD size, char* buffer) {
    return ::GetCurrentDirectoryA(size, buffer);
}

template <>
DWORD get_current_directory<wchar_t>(DWORD size, wchar_t* buffer) {
    return ::GetCurrentDirectoryW(size, buffer);
}

template <typename CharT>
UINT get_system_directory(CharT* buffer, UINT size);

template <>
UINT get_system_directory<char>(char* buffer, UINT size) {
    return ::GetSystemDirectoryA(buffer, size);
}

template <>
UINT get_system_directory<wchar_t>(wchar_t* buffer, UINT size) {
    return ::GetSystemDirectoryW(buffer, size);
}

template <typename CharT>
UINT get_windows_directory(CharT* buffer, UINT size);

template <>
UINT get_windows_directory<char>(char* buffer, UINT size) {
    return ::GetWindowsDirectoryA(buffer, size);
}

template <>
UINT get_windows_directory<wchar_t>(wchar_t* buffer, UINT size) {
    return ::GetWindowsDirectoryW(buffer, size);
}

template <typename CharT>
DWORD get_module_file_name(HMODULE module, CharT* buffer, DWORD size);

template <>
DWORD get_module_file_name<char>(HMODULE module, char* buffer, DWORD size) {
    return ::GetModuleFileNameA(module, buffer, size);
}

template <>
DWORD get_module_file_name<wchar_t>(HMODULE module, wchar_t* buffer, DWORD size) {
    return ::GetModuleFileNameW(module, buffer, size);
}

template <typename CharT>
DWORD search_path(const CharT* path,
                  const CharT* file_name,
                  const CharT* extension,
                  DWORD buffer_size,
                  CharT* buffer,
                  CharT** file_part);

template <>
DWORD search_path<char>(const char* path,
                        const char* file_name,
                        const char* extension,
                        DWORD buffer_size,
                        char* buffer,
                        char** file_part) {
    return ::SearchPathA(path, file_name, extension, buffer_size, buffer, file_part);
}

template <>
DWORD search_path<wchar_t>(const wchar_t* path,
                           const wchar_t* file_name,
                           const wchar_t* extension,
                           DWORD buffer_size,
                           wchar_t* buffer,
                           wchar_t** file_part) {
    return ::SearchPathW(path, file_name, extension, buffer_size, buffer, file_part);
}

template <typename CharT>
std::basic_string<CharT> call_windows_string_api(auto&& fn) {
    DWORD size = 256;
    while(true) {
        std::basic_string<CharT> buffer(size, CharT('\0'));
        DWORD len = fn(buffer.data(), size);
        if(len == 0) {
            return {};
        }
        if(len < size) {
            buffer.resize(len);
            return buffer;
        }
        size = len + 1;
    }
}

template <typename CharT>
std::basic_string<CharT> get_current_directory_dynamic() {
    return call_windows_string_api<CharT>(
        [](CharT* buffer, DWORD size) { return get_current_directory<CharT>(size, buffer); });
}

template <typename CharT>
std::basic_string<CharT> get_system_directory_dynamic() {
    return call_windows_string_api<CharT>(
        [](CharT* buffer, DWORD size) { return get_system_directory<CharT>(buffer, size); });
}

template <typename CharT>
std::basic_string<CharT> get_windows_directory_dynamic() {
    return call_windows_string_api<CharT>(
        [](CharT* buffer, DWORD size) { return get_windows_directory<CharT>(buffer, size); });
}

template <typename CharT>
std::basic_string<CharT> get_environment_variable_dynamic(const CharT* name) {
    return call_windows_string_api<CharT>(
        [name](CharT* buffer, DWORD size) { return get_environment_variable<CharT>(name, buffer, size); });
}

template <typename CharT>
std::basic_string<CharT> get_module_directory() {
    auto module_path = call_windows_string_api<CharT>(
        [](CharT* buffer, DWORD size) { return get_module_file_name<CharT>(nullptr, buffer, size); });
    if(module_path.empty()) {
        return {};
    }
    auto last_sep = module_path.find_last_of(std::basic_string<CharT>{k_path_sep<CharT>, CharT('/')});
    if(last_sep == std::basic_string<CharT>::npos) {
        return {};
    }
    module_path.resize(last_sep);
    return module_path;
}

template <typename CharT>
std::vector<std::basic_string<CharT>> split_path_var(std::basic_string_view<CharT> value) {
    std::vector<std::basic_string<CharT>> segments;
    size_t start = 0;
    while(start <= value.size()) {
        auto split_pos = value.find(k_path_delimiter<CharT>, start);
        auto token =
            value.substr(start,
                         split_pos == std::basic_string_view<CharT>::npos ? value.size() - start
                                                                          : split_pos - start);
        if(!token.empty()) {
            segments.emplace_back(token);
        }
        if(split_pos == std::basic_string_view<CharT>::npos) {
            break;
        }
        start = split_pos + 1;
    }
    return segments;
}

template <typename CharT>
void push_if_non_empty(std::vector<std::basic_string<CharT>>& out,
                       std::basic_string<CharT> value) {
    if(!value.empty()) {
        out.push_back(std::move(value));
    }
}

template <typename CharT>
std::basic_string<CharT> get_current_drive_root() {
    auto cwd = get_current_directory_dynamic<CharT>();
    if(cwd.size() < 2 || cwd[1] != CharT(':')) {
        return {};
    }
    std::basic_string<CharT> root;
    root.push_back(cwd[0]);
    root.push_back(CharT(':'));
    root.push_back(k_path_sep<CharT>);
    return root;
}

template <typename CharT>
std::basic_string<CharT> join_search_paths(
    const std::vector<std::basic_string<CharT>>& search_paths) {
    std::basic_string<CharT> merged;
    for(const auto& path: search_paths) {
        if(path.empty()) {
            continue;
        }
        if(!merged.empty()) {
            merged.push_back(k_path_delimiter<CharT>);
        }
        merged.append(path);
    }
    return merged;
}

template <typename CharT>
std::basic_string<CharT> resolve_with_search_paths(
    std::basic_string_view<CharT> candidate,
    const std::vector<std::basic_string<CharT>>& search_paths) {
    if(candidate.empty()) {
        return {};
    }

    auto merged = join_search_paths(search_paths);
    DWORD size = 260;
    while(true) {
        std::basic_string<CharT> buffer(size, CharT('\0'));
        DWORD resolved_len = search_path<CharT>(merged.empty() ? nullptr : merged.c_str(),
                                                std::basic_string<CharT>(candidate).c_str(),
                                                k_exe_suffix<CharT>.data(),
                                                size,
                                                buffer.data(),
                                                nullptr);
        if(resolved_len == 0) {
            return std::basic_string<CharT>(candidate);
        }
        if(resolved_len < size) {
            buffer.resize(resolved_len);
            return buffer;
        }
        size = resolved_len + 1;
    }
}

template <typename CharT>
std::vector<std::basic_string<CharT>> build_application_name_search_paths() {
    std::vector<std::basic_string<CharT>> search_paths;
    search_paths.reserve(2);
    push_if_non_empty(search_paths, get_current_drive_root<CharT>());
    push_if_non_empty(search_paths, get_current_directory_dynamic<CharT>());
    return search_paths;
}

template <typename CharT>
std::vector<std::basic_string<CharT>> build_command_line_search_paths() {
    std::vector<std::basic_string<CharT>> search_paths;
    search_paths.reserve(64);

    auto module_dir = get_module_directory<CharT>();
    auto current_dir = get_current_directory_dynamic<CharT>();
    auto system_dir = get_system_directory_dynamic<CharT>();
    auto windows_dir = get_windows_directory_dynamic<CharT>();

    push_if_non_empty(search_paths, std::move(module_dir));
    push_if_non_empty(search_paths, std::move(current_dir));
    push_if_non_empty(search_paths, std::move(system_dir));

    if(!windows_dir.empty()) {
        auto system16_dir = windows_dir;
        if(system16_dir.back() != k_path_sep<CharT> && system16_dir.back() != CharT('/')) {
            system16_dir.push_back(k_path_sep<CharT>);
        }
        if constexpr(std::same_as<CharT, char>) {
            system16_dir.append("System");
        } else {
            system16_dir.append(L"System");
        }
        push_if_non_empty(search_paths, std::move(system16_dir));
    }

    push_if_non_empty(search_paths, windows_dir);

    std::basic_string<CharT> path_value;
    if constexpr(std::same_as<CharT, char>) {
        path_value = get_environment_variable_dynamic<char>("PATH");
    } else {
        path_value = get_environment_variable_dynamic<wchar_t>(L"PATH");
    }
    for(auto& segment: split_path_var<CharT>(path_value)) {
        push_if_non_empty(search_paths, std::move(segment));
    }
    return search_paths;
}

}  // namespace

template <typename CharT>
std::basic_string<CharT>
resolve_application_name(std::basic_string_view<CharT> application_name) {
    return resolve_with_search_paths(application_name, build_application_name_search_paths<CharT>());
}

template <typename CharT>
std::basic_string<CharT> resolve_command_line_token(std::basic_string_view<CharT> token) {
    return resolve_with_search_paths(token, build_command_line_search_paths<CharT>());
}

template std::basic_string<char> resolve_application_name(std::basic_string_view<char>);
template std::basic_string<wchar_t> resolve_application_name(std::basic_string_view<wchar_t>);
template std::basic_string<char> resolve_command_line_token(std::basic_string_view<char>);
template std::basic_string<wchar_t> resolve_command_line_token(std::basic_string_view<wchar_t>);

}  // namespace catter::hook::shared::resolver

#endif
