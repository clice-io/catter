#include "capture/inject/windows/inject.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <cpptrace/exceptions.hpp>

// clang-format off
// <detours.h> needs the architecture macros of <windows.h>.
#include "support/winapi/win32.h"
#include <detours.h>
// clang-format on

#include "support/crossplat.h"
#include "support/encoding.h"
#include "support/exception.h"
#include "support/log.h"

namespace catter::proxy::hook {

namespace {

/// The machines catter has a hook for, catter-hook-<arch>.dll beside
/// catter-proxy.
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
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   "Failed to get the machine of the process");
    }
    return wow64_machine == IMAGE_FILE_MACHINE_UNKNOWN ? native_machine : wow64_machine;
}

/// Detours makes the header of a 32-bit .NET executable that runs as a 64-bit
/// process a 64-bit one of catter-proxy's own machine, and the CLR refuses to
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
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   "Failed to read the headers Detours saved in the process");
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
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   "Failed to write the machine of the process's header");
    }
}

/// `path` by its 8.3 short name; nothing when that is not ASCII, as on a volume
/// that keeps no short names.
std::optional<std::string> ascii_path(const std::filesystem::path& path) {
    const DWORD size = GetShortPathNameW(path.c_str(), nullptr, 0);
    if(size == 0) {
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   std::format("Failed to find {}", util::to_utf8(path)));
    }
    std::wstring short_path(size, L'\0');
    short_path.resize(GetShortPathNameW(path.c_str(), short_path.data(), size));
    if(!std::ranges::all_of(short_path, [](wchar_t c) { return c < 0x80; })) {
        return std::nullopt;
    }
    return std::string(short_path.begin(), short_path.end());
}

/// A copy of `dll` in catter's data directory, one per build of the DLL.
std::filesystem::path temporary_copy(const std::filesystem::path& dll) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if(!GetFileAttributesExW(dll.c_str(), GetFileExInfoStandard, &data)) {
        throw catter::system_error(GetLastError(),
                                   std::system_category(),
                                   std::format("Failed to find {}", util::to_utf8(dll)));
    }
    const auto build = std::format(L"{}|{}|{}|{}",
                                   dll.native(),
                                   data.nFileSizeLow,
                                   data.ftLastWriteTime.dwHighDateTime,
                                   data.ftLastWriteTime.dwLowDateTime);
    const auto dir = util::get_catter_data_path() /
                     std::format("hook-{:016x}", std::hash<std::wstring>{}(build));
    const auto copy = dir / dll.filename();
    if(std::filesystem::exists(copy)) {
        return copy;
    }

    // Written under a name of its own, then renamed: a process another
    // catter-proxy starts meanwhile never loads half a DLL.
    std::filesystem::create_directories(dir);
    auto partial = copy;
    partial += std::format(".{}", GetCurrentProcessId());
    std::filesystem::copy_file(dll, partial, std::filesystem::copy_options::overwrite_existing);
    if(!MoveFileExW(partial.c_str(), copy.c_str(), 0)) {
        const DWORD error = GetLastError();
        std::filesystem::remove(partial);
        // Another catter-proxy renamed its copy first.
        if(!std::filesystem::exists(copy)) {
            throw catter::system_error(
                error,
                std::system_category(),
                std::format("Failed to copy {} to {}", util::to_utf8(dll), util::to_utf8(copy)));
        }
    }
    return copy;
}

/// `dll` as Detours writes it into the import table. The loader reads that
/// name in the process's own ANSI code page, which a program can make UTF-8:
/// only ASCII reads the same in every process.
std::string import_name(const std::filesystem::path& dll) {
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
                    util::to_utf8(dll),
                    util::to_utf8(copy)));
}

}  // namespace

void inject(HANDLE process, std::string_view executable) {
    const USHORT machine = process_machine(process);
    const auto hook = std::ranges::find(HOOKS, machine, &Hook::machine);
    if(hook == std::ranges::end(HOOKS)) {
        LOG_WARN("No hook for machine 0x{:04X}: {} runs, but what it starts is not captured",
                 machine,
                 executable);
        return;
    }

    const auto dll =
        import_name(util::get_catter_root_path() / std::format("catter-hook-{}.dll", hook->arch));
    LPCSTR dlls[] = {dll.c_str()};
    if(!DetourUpdateProcessWithDll(process, dlls, 1)) {
        throw catter::system_error(
            GetLastError(),
            std::system_category(),
            std::format("Failed to add {} to the imports of {}", dll, executable));
    }
    if(machine != DETOURS_MACHINE) {
        fix_converted_header(process, machine);
    }
}

}  // namespace catter::proxy::hook
