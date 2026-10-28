#pragma once

#include <cstdint>
#include <string_view>

#ifdef CATTER_LINUX
#include <algorithm>
#include <charconv>
#include <format>
#include <iterator>
#include <span>
#include <string>
#include <system_error>
#endif

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

#ifdef CATTER_LINUX
/// The hash of `arguments`, FNV-1a's of each with a NUL after it.
template <typename Arguments>
std::uint64_t arguments_hash(const Arguments& arguments) noexcept {
    std::uint64_t hash = 0xcbf29ce484222325;
    for(const auto& argument: arguments) {
        for(char c: std::string_view(argument)) {
            hash = (hash ^ static_cast<unsigned char>(c)) * 0x100000001b3;
        }
        hash *= 0x100000001b3;
    }
    return hash;
}

/// env::DECIDED for the program catter-proxy `pid` starts with `argv`,
/// argv[0] among them: the number and the hash of the arguments past argv[0],
/// which the program's argv ends with however the kernel runs it.
template <typename Argv>
std::string decided_exec(int pid, const Argv& argv) {
    const auto args = std::span(argv).subspan(std::min<std::size_t>(std::size(argv), 1));
    return std::format("{}:{}:{:x}", pid, args.size(), arguments_hash(args));
}

/// env::DECIDED for the program of a command reported late, which catter-proxy
/// `pid` starts: its parent alone, so that its hook does not restart it again.
inline std::string decided_exec(int pid) {
    return std::to_string(pid);
}

/// Whether `decided`, env::DECIDED, stands for the exec of this process, whose
/// parent is `parent` and whose argv is `argv`.
inline bool is_decided(std::string_view decided, int parent, std::span<char* const> argv) noexcept {
    const auto* end = decided.data() + decided.size();
    int pid = 0;
    auto [rest, error] = std::from_chars(decided.data(), end, pid);
    if(error != std::errc() || pid != parent) {
        return false;
    }
    if(rest == end) {
        return true;
    }
    std::size_t count = 0;
    std::uint64_t hash = 0;
    auto counted = std::from_chars(rest + 1, end, count);
    if(*rest != ':' || counted.ec != std::errc() || counted.ptr == end || *counted.ptr != ':' ||
       std::from_chars(counted.ptr + 1, end, hash, 16).ec != std::errc()) {
        return false;
    }
    return argv.size() > count && arguments_hash(argv.last(count)) == hash;
}
#endif

#ifdef CATTER_WINDOWS
/// The session the hook in a process reports the commands it starts to,
/// which catter copies into each process it injects the hook into, as a
/// Detours payload of SESSION_GUID. The same in an x64 and an ARM64 process.
struct Session {
    /// The command the process runs, which those it starts are children of.
    std::int32_t id;

    /// The pipe catter listens on.
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
