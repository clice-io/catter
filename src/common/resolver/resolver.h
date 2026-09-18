#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace catter::hook::shared::resolver {

#ifdef CATTER_WINDOWS

// https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw
//
// The lpApplicationName parameter can be NULL. In that case, the module name must be the first
// white space–delimited token in the lpCommandLine string. If you are using a long file name that
// contains a space, use quoted strings to indicate where the file name ends and the arguments
// begin; If the file name does not contain an extension, .exe is appended. Therefore, if the file
// name extension is .com, this parameter must include the .com extension. If the file name ends in
// a period (.) with no extension, or if the file name contains a path, .exe is not appended. If the
// file name does not contain a directory path, the system searches for the executable file in the
// following sequence:
//
// 1.The directory from which the application loaded.
//
// 2.The current directory for the parent process.
//
// 3.The 32-bit Windows system directory. Use the GetSystemDirectory function to get the path of
// this directory.
//
// 4.The 16-bit Windows system directory. There is no function that obtains the path of this
// directory, but it is searched. The name of this directory is System.
//
// 5.The Windows directory. Use the GetWindowsDirectory function to get the path of this directory.
//
// 6.The directories that are listed in the PATH environment variable. Note that this function does
// not search the per-application path specified by the App Paths registry key. To include this
// per-application path in the search sequence, use the ShellExecute function.

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
//
// All other exec() functions (which do not include 'p' in the suffix) take as their first argument
// a (relative or absolute) pathname that identifies the program to be executed.
//
// pathname resolution: https://man7.org/linux/man-pages/man7/path_resolution.7.html

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
