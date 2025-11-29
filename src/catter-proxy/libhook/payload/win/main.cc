#include <array>
#include <cstddef>
#include <format>
#include <string_view>
#include <print>

#include <windows.h>
#include <detours.h>

#include "libhook/win/env.h"

// https://github.com/microsoft/Detours/wiki/DetourCreateProcessWithDll#remarks
#pragma comment(linker, "/export:DetourFinishHelperProcess,@1,NONAME")

namespace catter::win {
namespace {

HINSTANCE& dll_instance() {
    static HINSTANCE instance = nullptr;
    return instance;
}

std::filesystem::path current_path(HMODULE h = nullptr) {

    std::vector<char> data;
    data.resize(MAX_PATH);

    while(true) {
        if(GetModuleFileNameA(h, data.data(), data.size()) == data.size() &&
           GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            data.resize(data.size() * 2);
        } else {
            break;
        }
    }

    return std::filesystem::path(data.data()).parent_path();
}

}  // namespace

}  // namespace catter::win

// Use anonymous namespace to avoid exporting symbols
namespace {

namespace detour {
struct CreateProcessA {
    inline static char name[] = "CreateProcessA";
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

        return target(lpApplicationName,
                      lpCommandLine,
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

struct CreateProcessW {
    inline static char name[] = "CreateProcessW";
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

        return target(lpApplicationName,
                      lpCommandLine,
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

struct CreateProcessAsUserA {
    inline static char name[] = "CreateProcessAsUserA";
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

        return target(hToken,
                      lpApplicationName,
                      lpCommandLine,
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

struct CreateProcessAsUserW {
    inline static char name[] = "CreateProcessAsUserW";
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
        return target(hToken,
                      lpApplicationName,
                      lpCommandLine,
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

struct detour_meta {
    std::string_view name;
    void** target;
    void* detour;
};

template <typename... args_t>
auto collect_fn() noexcept {
    return std::array<detour_meta, sizeof...(args_t)>{
        detour_meta{args_t::name, (void**)(&args_t::target), (void*)(&args_t::detour)}
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
        std::println("Attaching detour for {}", m.name);
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
