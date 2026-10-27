#pragma once

#include <cstdint>

#ifdef CATTER_WINDOWS
#include <windows.h>
#endif

/// The hook catter loads into the processes of the build: on Linux and macOS
/// the library the loader preloads into each, on Windows the DLL catter adds
/// to each process the hook starts, and the session catter gives it there.
namespace catter::hook {

#if defined(CATTER_LINUX)
constexpr char LIBRARY[] = "libcatter-hook-unix.so";
constexpr char PRELOAD[] = "LD_PRELOAD";
/// What the loader splits PRELOAD at, with no way to escape them, the first of
/// which catter writes: a path that holds one cannot be preloaded.
constexpr char PRELOAD_SEPARATORS[] = ": ";
constexpr char PRELOAD_SEPARATORS_NAMED[] = "spaces and colons";
#elif defined(CATTER_MAC)
constexpr char LIBRARY[] = "libcatter-hook-unix.dylib";
constexpr char PRELOAD[] = "DYLD_INSERT_LIBRARIES";
constexpr char PRELOAD_SEPARATORS[] = ":";
constexpr char PRELOAD_SEPARATORS_NAMED[] = "colons";
#endif

#ifdef CATTER_WINDOWS
/// The session the hook in a process reports the commands it starts to,
/// which catter copies into each process it injects the hook into, as a
/// Detours payload of SESSION_GUID. The same in an x64 and an ARM64 process.
struct Session {
    /// The command the process runs, which those it starts are children of.
    std::int32_t id;

    /// The pipe catter listens on, as pipe_name() names it.
    char pipe[64];

    /// The command is not to run: the process ends with `exit_code` once the
    /// hook loads, before its program's own code runs. Its caller can still
    /// prepare it while it is suspended, as MSBuild's tracker does.
    bool ends;

    std::uint32_t exit_code;
};

constexpr GUID SESSION_GUID = {
    0x6f3c1b2a,
    0x58d4,
    0x4e0b,
    {0x9a, 0x7e, 0x21, 0xc4, 0x5d, 0x80, 0x3f, 0x16}
};
#endif

}  // namespace catter::hook
