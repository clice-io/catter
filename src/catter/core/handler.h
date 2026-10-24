#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <kota/async/runtime/task.h>

#include "util/data.h"

namespace catter::core {

/// How the build's stdout and stderr show: printed as it writes them, or only
/// kept for its result.
enum class StdioMode : std::uint8_t {
    inherit,
    capture,
};

/// The build a session runs.
struct Build {
    /// The build command, its program first.
    std::vector<std::string> command;

    std::string cwd;

    StdioMode stdio_mode;
};

/// What a handler decides for a command.
struct Action {
    enum class Type : std::uint8_t {
        /// Run the command as it is.
        Skip,
        /// Do not run the command: it exits 0.
        Drop,
        /// End the session with an error. Under inject it only fails its
        /// command, and the build goes on (catter#122).
        Abort,
        /// Run `command` in its place.
        Modify,
    };

    Type type = Type::Skip;

    /// For `Modify`, the command that runs instead.
    data::command command;
};

/// Decides what each command of a session does, and is told how the session
/// goes. A handler is created with its own arguments, before the session
/// starts.
///
/// - `on_start` comes first. When it returns no build, none runs, nothing
///   else is called, and catter exits 0.
/// - While the build runs, each command comes to `on_command`, or to
///   `on_error` when catter could not capture it, and to `on_exit` once it has
///   run. The calls of different commands interleave: one can start while
///   another is suspended.
/// - `on_finish` comes last, with how the build ended.
///
/// A replayed build runs nothing: there, the actions do nothing.
///
/// An exception a call throws ends the session: catter prints its message and
/// exits 1. Under inject, one from `on_command`, `on_error` or `on_exit` does
/// not: the build goes on (catter#122).
class Handler {
public:
    virtual ~Handler() = default;

    /// The build to run, changed or as it is, or none.
    virtual kota::task<std::optional<Build>> on_start(Build build) = 0;

    /// `parent` is the command that started this one: under inject, 0 for the
    /// build command; a replayed event may name none.
    virtual kota::task<Action> on_command(data::ipcid_t id,
                                          std::optional<data::ipcid_t> parent,
                                          data::command command) = 0;

    /// `message` says why catter could not capture the command.
    virtual kota::task<> on_error(data::ipcid_t id,
                                  std::optional<data::ipcid_t> parent,
                                  std::string message) = 0;

    virtual kota::task<> on_exit(data::ipcid_t id, data::process_result result) = 0;

    virtual kota::task<> on_finish(data::process_result result) = 0;
};

}  // namespace catter::core
