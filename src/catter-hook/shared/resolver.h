#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace catter::hook::shared::resolver {

#ifdef CATTER_WINDOWS

template <typename CharT>
std::basic_string<CharT> resolve_application_name(std::basic_string_view<CharT> application_name);

template <typename CharT>
std::basic_string<CharT> resolve_command_line_token(std::basic_string_view<CharT> token);

#else

[[nodiscard]]
std::expected<std::filesystem::path, int>
resolve_path_like(std::string_view file);

[[nodiscard]]
std::expected<std::filesystem::path, int>
resolve_from_search_path(std::string_view file, const char* search_path);

[[nodiscard]]
std::expected<std::filesystem::path, int>
resolve_from_path_env(std::string_view file, const char** envp);

#endif

}  // namespace catter::hook::shared::resolver
