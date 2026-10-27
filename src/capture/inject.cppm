module;

#include "modules/prelude.h"

#ifdef CATTER_WINDOWS
#include <windows.h>
#endif

#include "support/messages.h"

module catter:capture.inject;

namespace catter {

/// The options that start the command with the hook, which reports the
/// commands it starts to the session's pipe as children of command `id`.
kota::process::options hook_options(data::command command,
                                    data::ipcid_t id,
                                    std::string_view pipe_name);

#ifdef CATTER_WINDOWS
/// Adds the hook DLL of the suspended process's machine to its imports, so
/// that the loader maps it before the program's own code runs, which reports
/// the commands it starts to `pipe` as children of command `id`. A process of
/// a machine catter has no hook for runs without one, its commands
/// uncaptured: a warning names the program.
void inject_hook(HANDLE process,
                 std::string_view executable,
                 std::int32_t id,
                 std::string_view pipe);

/// Adds the hook DLL of the suspended process's machine to its imports, which
/// ends the process with exit code `code` once the loader maps it, before the
/// program's own code runs: for a command that is not to run, whose caller
/// may still prepare the process while it is suspended. False for a process
/// of a machine catter has no hook for.
bool inject_end(HANDLE process, std::string_view executable, std::uint32_t code);
#endif

}  // namespace catter
