#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <print>

#include <windows.h>
#include <detours.h>

#include "common.h"
#include "hook/windows/unique_file.h"
#include "hook/windows/env.h"

// https://github.com/microsoft/Detours/wiki/DetourCreateProcessWithDll#remarks
#pragma comment(linker, "/export:DetourFinishHelperProcess,@1,NONAME")

namespace catter::win {
namespace {

HINSTANCE& dll_instance() {
    static HINSTANCE instance = nullptr;
    return instance;
}

class path {
public:
    path() {
        std::tie(this->length, this->data) = catter::win::path(catter::win::dll_instance());
    }

    size_t size() const {
        return this->length;
    }

    const char* data_ptr() const {
        return this->data.get();
    }

    operator const char*() const {
        return this->data_ptr();
    }

    path(const path&) = delete;
    path(path&&) = delete;
    path& operator= (const path&) = delete;
    path& operator= (path&&) = delete;

    ~path() = default;

private:
    size_t length{};
    std::unique_ptr<char[]> data;
};

path& hook_dll_path() {
    static path dll_path{};
    return dll_path;
}
}  // namespace

}  // namespace catter::win

// Use anonymous namespace to avoid exporting symbols
namespace {

std::string wstring_to_utf8(const std::wstring& wstr, std::error_code& ec) {
    if(wstr.empty())
        return {};

    auto size_needed =
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);

    if(size_needed == 0) {
        switch(GetLastError()) {
            case ERROR_NO_UNICODE_TRANSLATION:
                ec = std::make_error_code(std::errc::illegal_byte_sequence);
                break;
            default: ec = std::make_error_code(std::errc::io_error);
        }
        return {};
    }

    std::string to(size_needed, 0);

    auto written = WideCharToMultiByte(CP_UTF8,
                                       0,
                                       &wstr[0],
                                       (int)wstr.size(),
                                       &to[0],
                                       size_needed,
                                       NULL,
                                       NULL);
    if(written == 0) {
        switch(GetLastError()) {
            case ERROR_NO_UNICODE_TRANSLATION:
                ec = std::make_error_code(std::errc::illegal_byte_sequence);
                break;
            default: ec = std::make_error_code(std::errc::io_error);
        }
        return {};
    }
    ec.clear();
    return to;
}

unique_file& output_file() {
    static unique_file instance;
    return instance;
}

template <typename... args_t>
void wwriteln(std::wformat_string<args_t...> fmt, args_t&&... args) {
    std::error_code ec;
    auto message = wstring_to_utf8(std::format(fmt, std::forward<args_t>(args)...), ec);
    if(ec) {
        output_file().writeln("Failed to convert wide string to UTF-8: {}", ec.message());
    } else {
        output_file().writeln("{}", std::move(message));
    }
}

template <typename... args_t>
void writeln(std::format_string<args_t...> fmt, args_t&&... args) {
    output_file().writeln(fmt, std::forward<args_t>(args)...);
}

namespace detour {
struct CreateProcessA {
    inline static decltype(::CreateProcessA)* target = ::CreateProcessA;

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
        writeln("{} {}",
                lpApplicationName ? lpApplicationName : "",
                lpCommandLine ? lpCommandLine : "");

        return DetourCreateProcessWithDllExA(lpApplicationName,
                                             lpCommandLine,
                                             lpProcessAttributes,
                                             lpThreadAttributes,
                                             bInheritHandles,
                                             dwCreationFlags,
                                             lpEnvironment,
                                             lpCurrentDirectory,
                                             lpStartupInfo,
                                             lpProcessInformation,
                                             catter::win::hook_dll_path(),
                                             target);
    }
};

struct CreateProcessW {
    inline static decltype(::CreateProcessW)* target = ::CreateProcessW;

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
        wwriteln(L"{} {}",
                 lpApplicationName ? lpApplicationName : L"",
                 lpCommandLine ? lpCommandLine : L"");
        return DetourCreateProcessWithDllExW(lpApplicationName,
                                             lpCommandLine,
                                             lpProcessAttributes,
                                             lpThreadAttributes,
                                             bInheritHandles,
                                             dwCreationFlags,
                                             lpEnvironment,
                                             lpCurrentDirectory,
                                             lpStartupInfo,
                                             lpProcessInformation,
                                             catter::win::hook_dll_path(),
                                             target);
    }
};

struct CreateProcessAsUserA {
    inline static decltype(::CreateProcessAsUserA)* target = ::CreateProcessAsUserA;

    static BOOL WINAPI detour(HANDLE hToken,
                              LPCSTR lpApplicationName,
                              LPSTR lpCommandLine,
                              LPSECURITY_ATTRIBUTES lpProcessAttributes,
                              LPSECURITY_ATTRIBUTES lpThreadAttributes,
                              BOOL bInheritHandles,
                              DWORD dwCreationFlags,
                              LPVOID lpEnvironment,
                              LPCSTR lpCurrentDirectory,
                              LPSTARTUPINFOA lpStartupInfo,
                              LPPROCESS_INFORMATION lpProcessInformation) {

        PROCESS_INFORMATION backup;
        if(lpProcessInformation == NULL) {
            lpProcessInformation = &backup;
            ZeroMemory(&backup, sizeof(backup));
        }

        if(!target(hToken,
                   lpApplicationName,
                   lpCommandLine,
                   lpProcessAttributes,
                   lpThreadAttributes,
                   bInheritHandles,
                   dwCreationFlags | CREATE_SUSPENDED,
                   lpEnvironment,
                   lpCurrentDirectory,
                   lpStartupInfo,
                   lpProcessInformation)) {
            return FALSE;
        }

        LPCSTR szDll = catter::win::hook_dll_path();

        if(!DetourUpdateProcessWithDll(lpProcessInformation->hProcess, &szDll, 1) &&
           !DetourProcessViaHelperA(lpProcessInformation->dwProcessId,
                                    catter::win::hook_dll_path(),
                                    CreateProcessA::target)) {

            TerminateProcess(lpProcessInformation->hProcess, ~0u);
            CloseHandle(lpProcessInformation->hProcess);
            CloseHandle(lpProcessInformation->hThread);
            return FALSE;
        }

        if(!(dwCreationFlags & CREATE_SUSPENDED)) {
            ResumeThread(lpProcessInformation->hThread);
        }

        if(lpProcessInformation == &backup) {
            CloseHandle(lpProcessInformation->hProcess);
            CloseHandle(lpProcessInformation->hThread);
        }

        return TRUE;
    }
};

struct CreateProcessAsUserW {
    inline static decltype(::CreateProcessAsUserW)* target = ::CreateProcessAsUserW;

    static BOOL WINAPI detour(HANDLE hToken,
                              LPCWSTR lpApplicationName,
                              LPWSTR lpCommandLine,
                              LPSECURITY_ATTRIBUTES lpProcessAttributes,
                              LPSECURITY_ATTRIBUTES lpThreadAttributes,
                              BOOL bInheritHandles,
                              DWORD dwCreationFlags,
                              LPVOID lpEnvironment,
                              LPCWSTR lpCurrentDirectory,
                              LPSTARTUPINFOW lpStartupInfo,
                              LPPROCESS_INFORMATION lpProcessInformation) {

        PROCESS_INFORMATION backup;
        if(lpProcessInformation == NULL) {
            lpProcessInformation = &backup;
            ZeroMemory(&backup, sizeof(backup));
        }

        if(!target(hToken,
                   lpApplicationName,
                   lpCommandLine,
                   lpProcessAttributes,
                   lpThreadAttributes,
                   bInheritHandles,
                   dwCreationFlags | CREATE_SUSPENDED,
                   lpEnvironment,
                   lpCurrentDirectory,
                   lpStartupInfo,
                   lpProcessInformation)) {
            return FALSE;
        }

        LPCSTR szDll = catter::win::hook_dll_path();

        if(!DetourUpdateProcessWithDll(lpProcessInformation->hProcess, &szDll, 1) &&
           !DetourProcessViaHelperW(lpProcessInformation->dwProcessId,
                                    catter::win::hook_dll_path(),
                                    CreateProcessW::target)) {

            TerminateProcess(lpProcessInformation->hProcess, ~0u);
            CloseHandle(lpProcessInformation->hProcess);
            CloseHandle(lpProcessInformation->hThread);
            return FALSE;
        }

        if(!(dwCreationFlags & CREATE_SUSPENDED)) {
            ResumeThread(lpProcessInformation->hThread);
        }

        if(lpProcessInformation == &backup) {
            CloseHandle(lpProcessInformation->hProcess);
            CloseHandle(lpProcessInformation->hThread);
        }

        return TRUE;
    }
};

struct detour_meta {
    std::string_view name;
    void** target;
    void* detour;
};

template <typename... args_t>
auto collect_fn() noexcept {
    return std::array<detour_meta, sizeof...(args_t)>{
        detour_meta{meta::type_name<args_t>(),
                    (void**)(&args_t::target),
                    (void*)(&args_t::detour)}
        ...
    };
}

auto& fn() noexcept {
    static auto instance =
        collect_fn<CreateProcessA, CreateProcessW, CreateProcessAsUserA, CreateProcessAsUserW>();
    return instance;
}

void attach() noexcept {
    for(auto& m: detour::fn()) {
        std::println("Attaching hook for `{}`", m.name);
        DetourAttach(m.target, m.detour);
    }
}

void detach() noexcept {
    for(auto& m: detour::fn()) {
        DetourDetach(m.target, m.detour);
    }
}
}  // namespace detour

}  // namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
    if(DetourIsHelperProcess()) {
        return TRUE;
    }

    if(dwReason == DLL_PROCESS_ATTACH) {
        catter::win::dll_instance() = hinst;

        DetourRestoreAfterWith();

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        detour::attach();

        DetourTransactionCommit();
    } else if(dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        detour::detach();

        DetourTransactionCommit();
    }
    return TRUE;
}
