#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace catter::hook {

/// The program CreateProcessW starts for `application_name` and
/// `command_line`, either of which may be null: the application name, from
/// the current directory, or else the first token of the command line, looked
/// for as CreateProcessW looks for it; none when there is no such file.
std::optional<std::string> resolve_program(const wchar_t* application_name,
                                           const wchar_t* command_line);

/// The arguments the program gets: those of `command_line`, or, when it is
/// null, of `application_name`, which CreateProcessW passes as the command
/// line then.
std::vector<std::string> arguments_of(const wchar_t* application_name, const wchar_t* command_line);

/// The variables of environment block `block`, as `NAME=value`: UTF-16 when
/// `unicode`, else in the ANSI code page, as CreateProcessW reads it; this
/// process's when it is null.
std::vector<std::string> environment_of(const void* block, bool unicode);

/// The environment block of `variables`, in UTF-16.
std::wstring environment_block(std::span<const std::string> variables);

/// `directory` made absolute, as CreateProcessW takes it; this process's
/// current directory when it is null.
std::string directory_of(const wchar_t* directory);

}  // namespace catter::hook
