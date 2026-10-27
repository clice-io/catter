#pragma once

#include <cstdint>
#include <windows.h>

namespace catter::config {

/// The session the hook in a process reports the commands it starts to,
/// which catter copies into each process it injects the hook into, as a
/// Detours payload of HOOK_SESSION_GUID. The same in an x64 and an ARM64
/// process.
struct HookSession {
    /// The command the process runs, which those it starts are children of.
    std::int32_t id;

    /// The pipe catter listens on, as session_pipe_name() names it.
    char pipe[64];

    /// The command is not to run: the process ends with `exit_code` once the
    /// hook loads, before its program's own code runs. Its caller can still
    /// prepare it while it is suspended, as MSBuild's tracker does.
    bool ends;

    std::uint32_t exit_code;
};

constexpr GUID HOOK_SESSION_GUID = {
    0x6f3c1b2a,
    0x58d4,
    0x4e0b,
    {0x9a, 0x7e, 0x21, 0xc4, 0x5d, 0x80, 0x3f, 0x16}
};

}  // namespace catter::config
