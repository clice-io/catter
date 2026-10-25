#include <algorithm>
#include <cstring>
#include <optional>
#include <string>

// clang-format off
// <detours.h> needs the architecture macros of <windows.h>.
#include "support/winapi/win32.h"
#include <detours.h>
// clang-format on

#include "hook/windows/util.h"

// Use anonymous namespace to avoid exporting symbols
namespace {

namespace hooks {

struct CreateProcessW {
    inline static decltype(::CreateProcessW)* original = &::CreateProcessW;

    static BOOL WINAPI detour(LPCWSTR lpApplicationName,
                              LPWSTR lpCommandLine,
                              LPSECURITY_ATTRIBUTES lpProcessAttributes,
                              LPSECURITY_ATTRIBUTES lpThreadAttributes,
                              BOOL bInheritHandles,
                              DWORD dwCreationFlags,
                              LPVOID lpEnvironment,
                              LPCWSTR lpCurrentDirectory,
                              LPSTARTUPINFOW lpStartupInfo,
                              LPPROCESS_INFORMATION lpProcessInformation) {

        auto converted_cmdline =
            catter::hook::build_proxy_command<wchar_t>(catter::hook::get_proxy_path<wchar_t>(),
                                                       catter::hook::get_ipc_id<wchar_t>(),
                                                       catter::hook::get_ipc_pipe<wchar_t>(),
                                                       lpApplicationName,
                                                       lpCommandLine);

        return original(nullptr,
                        converted_cmdline.data(),
                        lpProcessAttributes,
                        lpThreadAttributes,
                        bInheritHandles,
                        dwCreationFlags,
                        lpEnvironment,
                        lpCurrentDirectory,
                        lpStartupInfo,
                        lpProcessInformation);
    }
};

struct CreateProcessA {
    inline static decltype(::CreateProcessA)* original = &::CreateProcessA;

    /// The program's narrow strings are in its ANSI code page, which holds few
    /// characters past ASCII: they go on as UTF-16, through CreateProcessW, so
    /// that the command reaches catter-proxy as the program wrote it.
    static BOOL WINAPI detour(LPCSTR lpApplicationName,
                              LPSTR lpCommandLine,
                              LPSECURITY_ATTRIBUTES lpProcessAttributes,
                              LPSECURITY_ATTRIBUTES lpThreadAttributes,
                              BOOL bInheritHandles,
                              DWORD dwCreationFlags,
                              LPVOID lpEnvironment,
                              LPCSTR lpCurrentDirectory,
                              LPSTARTUPINFOA lpStartupInfo,
                              LPPROCESS_INFORMATION lpProcessInformation) {
        auto application_name = widen(lpApplicationName);
        auto command_line = widen(lpCommandLine);
        auto current_directory = widen(lpCurrentDirectory);
        auto desktop = widen(lpStartupInfo->lpDesktop);
        auto title = widen(lpStartupInfo->lpTitle);

        // STARTUPINFOA and STARTUPINFOW, and their EX forms, differ only in
        // the type their strings point to.
        STARTUPINFOEXW startup{};
        std::memcpy(&startup,
                    lpStartupInfo,
                    (std::min)(sizeof(startup), size_t(lpStartupInfo->cb)));
        startup.StartupInfo.lpReserved = nullptr;
        startup.StartupInfo.lpDesktop = desktop ? desktop->data() : nullptr;
        startup.StartupInfo.lpTitle = title ? title->data() : nullptr;

        return CreateProcessW::detour(application_name ? application_name->c_str() : nullptr,
                                      command_line ? command_line->data() : nullptr,
                                      lpProcessAttributes,
                                      lpThreadAttributes,
                                      bInheritHandles,
                                      dwCreationFlags,
                                      lpEnvironment,
                                      current_directory ? current_directory->c_str() : nullptr,
                                      &startup.StartupInfo,
                                      lpProcessInformation);
    }

private:
    /// A narrow string of the program as UTF-16, in the code page its file
    /// APIs use; null stays null.
    static std::optional<std::wstring> widen(const char* text) {
        if(text == nullptr) {
            return std::nullopt;
        }
        const UINT code_page = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
        const int length = MultiByteToWideChar(code_page, 0, text, -1, nullptr, 0);
        std::wstring wide(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(code_page, 0, text, -1, wide.data(), length);
        wide.pop_back();
        return wide;
    }
};

/// Detours points each hook's `original` at the trampoline to the function
/// it replaces.
template <typename... Hooks>
LONG attach() noexcept {
    DetourTransactionBegin();
    (DetourAttach(reinterpret_cast<void**>(&Hooks::original),
                  reinterpret_cast<void*>(&Hooks::detour)),
     ...);
    return DetourTransactionCommit();
}
}  // namespace hooks
}  // namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, [[maybe_unused]] LPVOID reserved) {
    // The DLL is an import of the executable, unloaded only with the process:
    // nothing to undo on DLL_PROCESS_DETACH.
    if(dwReason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }
    // avoid the DLL_THREAD_ATTACH and DLL_THREAD_DETACH notifications to reduce overhead
    DisableThreadLibraryCalls(hinst);
    // Undo the import catter-proxy added to the executable's import table.
    DetourRestoreAfterWith();
    return hooks::attach<hooks::CreateProcessA, hooks::CreateProcessW>() == NO_ERROR;
}
