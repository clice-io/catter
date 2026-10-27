#include <algorithm>
#include <cstring>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <system_error>
#include <kota/support/utf16.h>
#include <kota/sys/windows.h>

// clang-format off
// <detours.h> needs the architecture macros of <windows.h>.
#include <windows.h>
#include <detours.h>
// clang-format on

#include "hook/windows/connection.h"
#include "hook/windows/create_process.h"
#include "support/hook.h"
#include "support/messages.h"
#include "support/program.h"

// Use anonymous namespace to avoid exporting symbols
namespace {

/// The session this process reports the programs it starts to.
catter::hook::Session session;

namespace hooks {

/// Runs `create`, which no exception leaves: only an allocation throws, which
/// fails the call as CreateProcess fails it.
template <typename Create>
BOOL guarded(Create&& create) noexcept {
    try {
        return create();
    } catch(...) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
}

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
        return guarded([&] {
            return start(lpApplicationName,
                         lpCommandLine,
                         lpProcessAttributes,
                         lpThreadAttributes,
                         bInheritHandles,
                         dwCreationFlags,
                         lpEnvironment,
                         lpCurrentDirectory,
                         lpStartupInfo,
                         lpProcessInformation);
        });
    }

private:
    /// Starts the program as catter decides, with the caller's own flags and
    /// handles: suspended, while catter injects the hook into it, and resumed
    /// unless the caller asked for it suspended. The caller gets the program
    /// itself.
    static BOOL start(LPCWSTR lpApplicationName,
                      LPWSTR lpCommandLine,
                      LPSECURITY_ATTRIBUTES lpProcessAttributes,
                      LPSECURITY_ATTRIBUTES lpThreadAttributes,
                      BOOL bInheritHandles,
                      DWORD dwCreationFlags,
                      LPVOID lpEnvironment,
                      LPCWSTR lpCurrentDirectory,
                      LPSTARTUPINFOW lpStartupInfo,
                      LPPROCESS_INFORMATION lpProcessInformation) {
        auto create_process = [&](LPCWSTR application_name,
                                  LPWSTR command_line,
                                  DWORD flags,
                                  LPVOID environment,
                                  LPCWSTR current_directory) {
            return original(application_name,
                            command_line,
                            lpProcessAttributes,
                            lpThreadAttributes,
                            bInheritHandles,
                            flags,
                            environment,
                            current_directory,
                            lpStartupInfo,
                            lpProcessInformation);
        };
        auto unchanged = [&] {
            return create_process(lpApplicationName,
                                  lpCommandLine,
                                  dwCreationFlags,
                                  lpEnvironment,
                                  lpCurrentDirectory);
        };

        const auto program = catter::hook::resolve_program(lpApplicationName, lpCommandLine);
        if(!program) {
            return unchanged();
        }
        // A program that outlives its session, as a build server does, starts
        // what it starts as it would without catter.
        auto connection = catter::hook::connect(session.pipe);
        if(!connection) {
            return unchanged();
        }
        const catter::data::command sent{
            .cwd = catter::hook::directory_of(lpCurrentDirectory),
            .executable = *program,
            .args = catter::hook::arguments_of(lpApplicationName, lpCommandLine),
            .env =
                catter::hook::environment_of(lpEnvironment,
                                             (dwCreationFlags & CREATE_UNICODE_ENVIRONMENT) != 0),
        };
        using MakeDecision = catter::ipc::Request<catter::ipc::RequestType::MAKE_DECISION>;
        using ReportError = catter::ipc::Request<catter::ipc::RequestType::REPORT_ERROR>;
        using Started = catter::ipc::Request<catter::ipc::RequestType::STARTED>;
        const auto decision = connection->send_request<MakeDecision::Result>(
            MakeDecision::method,
            MakeDecision::Params{.parent = session.id, .command = sent});
        if(!decision) {
            return unchanged();
        }

        // What a handler changed is written anew, the rest passed on as the
        // caller wrote it.
        const auto& command = decision->action.cmd;
        auto application_name = lpApplicationName;
        auto command_line = lpCommandLine;
        auto environment = lpEnvironment;
        auto current_directory = lpCurrentDirectory;
        auto flags = dwCreationFlags | CREATE_SUSPENDED;
        std::wstring edited_application_name;
        std::wstring edited_command_line;
        std::wstring edited_environment;
        std::wstring edited_current_directory;
        if(command.executable != sent.executable || command.args != sent.args) {
            kota::to_utf16(edited_application_name,
                           catter::resolve_program(command.executable, command.cwd));
            application_name = edited_application_name.c_str();
        }
        if(command.args != sent.args) {
            std::string line;
            kota::sys::windows::join_command_line(line, command.args);
            kota::to_utf16(edited_command_line, line);
            command_line = edited_command_line.data();
        }
        if(command.env != sent.env) {
            edited_environment = catter::hook::environment_block(command.env);
            environment = edited_environment.data();
            flags |= CREATE_UNICODE_ENVIRONMENT;
        }
        if(command.cwd != sent.cwd) {
            kota::to_utf16(edited_current_directory, command.cwd);
            current_directory = edited_current_directory.c_str();
        }

        if(!create_process(application_name, command_line, flags, environment, current_directory)) {
            const DWORD error = GetLastError();
            connection->send_request<ReportError::Result>(
                ReportError::method,
                ReportError::Params{
                    .parent = session.id,
                    .message = std::format(
                        "`{}` failed to start: {}",
                        std::ranges::to<std::string>(command.args | std::views::join_with(' ')),
                        std::system_category().message(static_cast<int>(error))),
                });
            SetLastError(error);
            return FALSE;
        }
        const auto runs = connection->send_request<Started::Result>(
            Started::method,
            Started::Params{.pid = lpProcessInformation->dwProcessId});
        if(runs.has_value() && !*runs) {
            TerminateProcess(lpProcessInformation->hProcess, 1);
        } else if(!(dwCreationFlags & CREATE_SUSPENDED)) {
            ResumeThread(lpProcessInformation->hThread);
        }
        return TRUE;
    }
};

struct CreateProcessA {
    inline static decltype(::CreateProcessA)* original = &::CreateProcessA;

    /// The program's narrow strings are in its ANSI code page, which holds few
    /// characters past ASCII: they go on as UTF-16, through CreateProcessW, so
    /// that the command reaches catter as the program wrote it.
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
        return guarded([&] {
            auto application_name = widen(lpApplicationName);
            auto command_line = widen(lpCommandLine);
            auto current_directory = widen(lpCurrentDirectory);
            auto desktop = widen(lpStartupInfo->lpDesktop);
            auto title = widen(lpStartupInfo->lpTitle);

            // STARTUPINFOA and STARTUPINFOW, and their EX forms, differ only
            // in the type their strings point to.
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
        });
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
    // Undo the import catter added to the executable's import table.
    DetourRestoreAfterWith();
    // catter killed between adding the DLL and copying its session leaves
    // none: the process runs as it would without catter.
    const void* found = DetourFindPayloadEx(catter::hook::SESSION_GUID, nullptr);
    if(found == nullptr) {
        return TRUE;
    }
    std::memcpy(&session, found, sizeof(session));
    if(session.ends) {
        TerminateProcess(GetCurrentProcess(), session.exit_code);
    }
    return hooks::attach<hooks::CreateProcessA, hooks::CreateProcessW>() == NO_ERROR;
}
