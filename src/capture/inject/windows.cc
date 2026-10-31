module;

#include "modules/prelude.h"

// clang-format off
// <detours.h> needs the architecture macros of <windows.h>.
#include <windows.h>
#include <detours.h>
// clang-format on

#include "support/hook.h"
#include "support/log.h"
#include "support/messages.h"
#include "support/paths.h"

module catter;

import :capture.inject;
import :session.process;

namespace catter {

namespace {

/// Throws that `what` failed, as `error` says: by default, the calling thread's
/// last error.
[[noreturn]] void fail(std::string_view what,
                       std::error_code error = {static_cast<int>(GetLastError()),
                                                std::system_category()}) {
    throw cpptrace::runtime_error(std::format("{}: {}", what, error.message()));
}

/// The machines catter has a hook for, catter-hook-<arch>.dll beside catter.
struct Hook {
    USHORT machine;
    std::string_view arch;
};

constexpr Hook HOOKS[] = {
    {.machine = IMAGE_FILE_MACHINE_AMD64, .arch = "x64"  },
    {.machine = IMAGE_FILE_MACHINE_ARM64, .arch = "arm64"},
};

/// The machine whose code the process runs. x64 processes emulated on ARM64,
/// and ARM64EC ones, run x64 code and load x64 DLLs: IsWow64Process2 tells
/// neither from a native ARM64 process, ProcessMachineTypeInfo (Windows 11)
/// does.
USHORT process_machine(HANDLE process) {
    PROCESS_MACHINE_INFORMATION info{};
    if(GetProcessInformation(process, ProcessMachineTypeInfo, &info, sizeof(info))) {
        return info.ProcessMachine;
    }

    // Windows 10 emulates no x64: a process runs the OS's machine unless it
    // is a WOW64 one.
    USHORT wow64_machine = IMAGE_FILE_MACHINE_UNKNOWN;
    USHORT native_machine = IMAGE_FILE_MACHINE_UNKNOWN;
    if(!IsWow64Process2(process, &wow64_machine, &native_machine)) {
        fail("Failed to get the machine of the process");
    }
    return wow64_machine == IMAGE_FILE_MACHINE_UNKNOWN ? native_machine : wow64_machine;
}

/// Detours makes the header of a 32-bit .NET executable that runs as a 64-bit
/// process a 64-bit one of catter's own machine, and the CLR refuses to
/// run an x64 process whose header says arm64, or the other way round.
#ifdef DETOURS_ARM64
constexpr USHORT DETOURS_MACHINE = IMAGE_FILE_MACHINE_ARM64;
#else
constexpr USHORT DETOURS_MACHINE = IMAGE_FILE_MACHINE_AMD64;
#endif

/// Gives a header Detours converted the process's machine, in the process and
/// in the copy Detours saved there for DetourRestoreAfterWith to put back.
void fix_converted_header(HANDLE process, USHORT machine) {
    auto* remote =
        static_cast<BYTE*>(DetourFindRemotePayload(process, DETOUR_EXE_RESTORE_GUID, nullptr));
    DETOUR_EXE_RESTORE saved{};
    if(remote == nullptr || !ReadProcessMemory(process, remote, &saved, sizeof(saved), nullptr)) {
        fail("Failed to read the headers Detours saved in the process");
    }
    // Detours converts only .NET executables, and saves the converted header.
    if(saved.pclr == nullptr || saved.inh.FileHeader.Machine == machine) {
        return;
    }

    constexpr auto machine_offset =
        offsetof(IMAGE_NT_HEADERS64, FileHeader) + offsetof(IMAGE_FILE_HEADER, Machine);
    auto* in_copy = remote + offsetof(DETOUR_EXE_RESTORE, inh) + machine_offset;
    auto* in_image = saved.pinh + machine_offset;
    DWORD protection = 0;
    if(!WriteProcessMemory(process, in_copy, &machine, sizeof(machine), nullptr) ||
       !VirtualProtectEx(process, in_image, sizeof(machine), PAGE_READWRITE, &protection) ||
       !WriteProcessMemory(process, in_image, &machine, sizeof(machine), nullptr) ||
       !VirtualProtectEx(process, in_image, sizeof(machine), protection, &protection)) {
        fail("Failed to write the machine of the process's header");
    }
}

/// `path` by its 8.3 short name; nothing when that is not ASCII, as on a volume
/// that keeps no short names.
std::optional<std::string> ascii_path(std::string_view path) {
    std::wstring wide;
    if(auto error = kotatsu::to_utf16(wide, path)) {
        fail(std::format("Failed to find {}", path), error);
    }
    const DWORD size = GetShortPathNameW(wide.c_str(), nullptr, 0);
    if(size == 0) {
        fail(std::format("Failed to find {}", path));
    }
    std::wstring short_path(size, L'\0');
    short_path.resize(GetShortPathNameW(wide.c_str(), short_path.data(), size));
    if(!std::ranges::all_of(short_path, [](wchar_t c) { return c < 0x80; })) {
        return std::nullopt;
    }
    return std::string(short_path.begin(), short_path.end());
}

/// A copy of `dll` in catter's data directory, one per build of the DLL.
std::string temporary_copy(std::string_view dll) {
    auto status = kotatsu::fs::status(dll);
    if(!status) {
        fail(std::format("Failed to find {}", dll), status.error());
    }
    const auto build =
        std::format("{}|{}|{}", dll, status->size, status->mtime.time_since_epoch().count());
    auto copy = data_path(std::format("hook-{:016x}", std::hash<std::string>{}(build)));
    kotatsu::fs::path::push(copy, kotatsu::fs::path::filename(dll));
    if(kotatsu::fs::is_file(copy)) {
        return copy;
    }

    // Written whole under a name of its own, then renamed, over nothing: a
    // process another catter starts meanwhile never loads half a DLL, nor
    // finds the one it loads renamed over.
    const auto partial = std::format("{}.{}", copy, GetCurrentProcessId());
    auto created = kotatsu::fs::create_directories(kotatsu::fs::path::parent(copy));
    auto error = created ? kotatsu::fs::copy_file(dll, partial) : created.error();
    if(!error) {
        error = kotatsu::fs::rename_noreplace(partial, copy);
        if(error) {
            kotatsu::fs::remove(partial);
        }
    }
    // Another catter renamed its copy first, which a process may have loaded
    // already.
    if(error && !kotatsu::fs::is_file(copy)) {
        fail(std::format("Failed to copy {} to {}", dll, copy), error);
    }
    return copy;
}

/// `dll` as Detours writes it into the import table. The loader reads that
/// name in the process's own ANSI code page, which a program can make UTF-8:
/// only ASCII reads the same in every process.
std::string import_name(std::string_view dll) {
    if(auto name = ascii_path(dll)) {
        return *name;
    }
    // catter's data directory is ASCII unless the user's name is not, and then
    // on the system volume, which keeps short names by default.
    const auto copy = temporary_copy(dll);
    if(auto name = ascii_path(copy)) {
        return *name;
    }
    throw cpptrace::runtime_error(
        std::format("Cannot load {} into a process: neither it nor its copy {} has an ASCII path",
                    dll,
                    copy));
}

/// Adds the hook DLL of the suspended process's machine to its imports, and
/// gives it `session`; false for a process of a machine catter has no hook
/// for.
bool add_hook(HANDLE process, std::string_view executable, const hook::Session& session) {
    const USHORT machine = process_machine(process);
    const auto hook = std::ranges::find(HOOKS, machine, &Hook::machine);
    if(hook == std::ranges::end(HOOKS)) {
        return false;
    }

    const auto dll = import_name(program_path(std::format("catter-hook-{}.dll", hook->arch)));
    LPCSTR dlls[] = {dll.c_str()};
    if(!DetourUpdateProcessWithDll(process, dlls, 1)) {
        fail(std::format("Failed to add {} to the imports of {}", dll, executable));
    }
    if(machine != DETOURS_MACHINE) {
        fix_converted_header(process, machine);
    }
    if(!DetourCopyPayloadToProcess(process, hook::SESSION_GUID, &session, sizeof(session))) {
        fail(std::format("Failed to give the hook in {} its session", executable));
    }
    return true;
}

}  // namespace

void inject_hook(HANDLE process,
                 std::string_view executable,
                 std::int32_t id,
                 std::string_view pipe) {
    hook::Session session{.id = id};
    pipe.copy(session.pipe, sizeof(session.pipe) - 1);
    if(!add_hook(process, executable, session)) {
        LOG_WARN("No hook for machine 0x{:04X}: {} runs, but what it starts is not captured",
                 process_machine(process),
                 executable);
    }
}

bool inject_end(HANDLE process, std::string_view executable, std::uint32_t code) {
    return add_hook(process, executable, {.ends = true, .exit_code = code});
}

kotatsu::async::process::options hook_options(data::command command,
                                              data::ipcid_t id,
                                              std::string_view pipe_name) {
    auto opts = process_options(command);
    opts.windows_before_start = [executable = std::move(command.executable),
                                 id,
                                 pipe = std::string(pipe_name)](void* process, void*) {
        inject_hook(static_cast<HANDLE>(process), executable, id, pipe);
        return std::error_code();
    };
    return opts;
}

}  // namespace catter
