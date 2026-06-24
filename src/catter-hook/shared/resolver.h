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

// The execlp(), execvp(), and execvpe() functions duplicate the actions of the shell in searching
// for an executable file if the specified filename does not contain a slash (/) character. The file
// is sought in the colon-separated list of directory pathnames specified in the PATH environment
// variable. If this variable isn't defined, the path list defaults to the current directory
// followed by the list of directories returned by confstr(_CS_PATH). (This confstr call typically
// returns the value "/bin:/usr/bin".)
//
// If the specified filename includes a slash character, then PATH is ignored, and the file at the
// specified pathname is executed.

[[nodiscard]]
std::expected<std::filesystem::path, int> resolve_path_like(std::string_view file);

[[nodiscard]]
std::expected<std::filesystem::path, int> resolve_from_search_path(std::string_view file,
                                                                   const char* search_path);

[[nodiscard]]
std::expected<std::filesystem::path, int> resolve_from_path_env(std::string_view file,
                                                                const char* path_env);

#endif

}  // namespace catter::hook::shared::resolver
