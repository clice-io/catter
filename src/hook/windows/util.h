#pragma once

#include <optional>
#include <string>

namespace catter::hook {

/// The program CreateProcessW starts for `application_name` and
/// `command_line`, either of which may be null: the application name, from
/// the current directory, or else the first token of the command line, looked
/// for as CreateProcessW looks for it; none when there is no such file.
std::optional<std::string> resolve_program(const wchar_t* application_name,
                                           const wchar_t* command_line);

/// The command line that starts catter-proxy, as the hook's environment names
/// it, in place of what CreateProcessW was asked to start; none when there is
/// no program to start, which CreateProcessW then fails as it does.
std::optional<std::wstring> proxy_command_line(const wchar_t* application_name,
                                               const wchar_t* command_line);

}  // namespace catter::hook
