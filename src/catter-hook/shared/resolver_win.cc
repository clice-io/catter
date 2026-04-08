#ifdef CATTER_WINDOWS

#include "shared/resolver.h"

#include <algorithm>
#include <concepts>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../win/payload/winapi.h"

namespace catter::hook::shared::resolver {
using namespace catter::win::payload;

namespace {
namespace detail {

template <CharT char_t>
constexpr char_t k_path_delimiter = char_t(';');

template <CharT char_t>
constexpr char_t k_path_sep = char_t('\\');

template <CharT char_t>
std::vector<std::basic_string<char_t>> split_path_var(std::basic_string_view<char_t> value) {
    std::vector<std::basic_string<char_t>> segments;
    size_t start = 0;
    while(start <= value.size()) {
        auto split_pos = value.find(k_path_delimiter<char_t>, start);
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

template <CharT char_t>
void push_if_non_empty(std::vector<std::basic_string<char_t>>& out,
                       std::basic_string<char_t> value) {
    if(!value.empty()) {
        out.push_back(std::move(value));
    }
}

template <CharT char_t>
std::basic_string<char_t> get_current_drive_root() {
    auto cwd = GetCurrentDirectoryDynamic<char_t>();
    if(cwd.size() < 2 || cwd[1] != char_t(':')) {
        return {};
    }

    std::basic_string<char_t> root;
    root.push_back(cwd[0]);
    root.push_back(char_t(':'));
    root.push_back(k_path_sep<char_t>);
    return root;
}

template <CharT char_t>
constexpr std::basic_string_view<char_t> k_path_env_name = {};

template <>
constexpr std::basic_string_view<char> k_path_env_name<char> = "PATH";

template <>
constexpr std::basic_string_view<wchar_t> k_path_env_name<wchar_t> = L"PATH";

template <CharT char_t>
constexpr std::basic_string_view<char_t> k_system16_name = {};

template <>
constexpr std::basic_string_view<char> k_system16_name<char> = "System";

template <>
constexpr std::basic_string_view<wchar_t> k_system16_name<wchar_t> = L"System";

template <CharT char_t>
constexpr std::basic_string_view<char_t> exe_suffix;
template <>
constexpr std::basic_string_view<char> exe_suffix<char> = ".exe";
template <>
constexpr std::basic_string_view<wchar_t> exe_suffix<wchar_t> = L".exe";
}  // namespace detail

template <CharT char_t>
class Resolver {
public:
    explicit Resolver(std::vector<std::basic_string<char_t>> search_paths) :
        m_search_paths(std::move(search_paths)) {}

    std::basic_string<char_t> resolve(std::basic_string_view<char_t> app_name) const {
        if(app_name.empty()) {
            return {};
        }

        auto original_name = std::basic_string<char_t>(app_name);

        auto search_paths = join_search_paths(m_search_paths);
        auto resolved =
            SearchPathDynamic<char_t>(search_paths.empty() ? nullptr : search_paths.c_str(),
                                      app_name,
                                      detail::exe_suffix<char_t>.data());
        if(!resolved.empty()) {
            return resolved;
        }

        // Keep original input when search paths do not resolve an existing file.
        return original_name;
    }

private:
    static std::basic_string<char_t>
        join_search_paths(const std::vector<std::basic_string<char_t>>& search_paths) {
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

    std::vector<std::basic_string<char_t>> m_search_paths;
};

template <CharT char_t>
Resolver<char_t> create_app_name_resolver() {
    std::vector<std::basic_string<char_t>> search_paths;
    search_paths.reserve(2);
    // Search order for explicit application_name:
    // 1) current drive root, 2) current directory.
    detail::push_if_non_empty(search_paths, detail::get_current_drive_root<char_t>());
    detail::push_if_non_empty(search_paths, GetCurrentDirectoryDynamic<char_t>());
    return Resolver<char_t>(std::move(search_paths));
}

template <CharT char_t>
Resolver<char_t> create_command_line_resolver() {
    std::vector<std::basic_string<char_t>> search_paths;
    search_paths.reserve(64);
    // Search order for command line token:
    // 1) directory of current process image.
    // 2) current directory.
    // 3) 32-bit system directory.
    // 4) 16-bit system directory named "System" under Windows directory.
    // 5) Windows directory.
    // 6) directories listed in PATH.
    auto module_dir = GetModuleDirectory<char_t>(nullptr);
    auto current_dir = GetCurrentDirectoryDynamic<char_t>();
    auto system_dir = GetSystemDirectoryDynamic<char_t>();
    auto windows_dir = GetWindowsDirectoryDynamic<char_t>();

    detail::push_if_non_empty(search_paths, std::move(module_dir));
    detail::push_if_non_empty(search_paths, std::move(current_dir));
    detail::push_if_non_empty(search_paths, std::move(system_dir));
    if(!windows_dir.empty()) {
        auto system16_dir = windows_dir;
        if(system16_dir.back() != detail::k_path_sep<char_t> &&
           system16_dir.back() != char_t('/')) {
            system16_dir.push_back(detail::k_path_sep<char_t>);
        }
        system16_dir.append(detail::k_system16_name<char_t>);
        detail::push_if_non_empty(search_paths, std::move(system16_dir));
    }
    detail::push_if_non_empty(search_paths, windows_dir);

    auto path_value =
        GetEnvironmentVariableDynamic<char_t>(detail::k_path_env_name<char_t>.data(), 4096);
    auto path_segments = detail::split_path_var<char_t>(path_value);
    for(auto& segment: path_segments) {
        detail::push_if_non_empty(search_paths, std::move(segment));
    }

    return Resolver<char_t>(std::move(search_paths));
}

}  // namespace

template <typename CharT>
std::basic_string<CharT> resolve_application_name(std::basic_string_view<CharT> application_name) {
    return create_app_name_resolver<CharT>().resolve(application_name);
}

template <typename CharT>
std::basic_string<CharT> resolve_command_line_token(std::basic_string_view<CharT> token) {
    return create_command_line_resolver<CharT>().resolve(token);
}

template std::basic_string<char> resolve_application_name(std::basic_string_view<char>);
template std::basic_string<wchar_t> resolve_application_name(std::basic_string_view<wchar_t>);
template std::basic_string<char> resolve_command_line_token(std::basic_string_view<char>);
template std::basic_string<wchar_t> resolve_command_line_token(std::basic_string_view<wchar_t>);

}  // namespace catter::hook::shared::resolver

#endif
