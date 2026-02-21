#include <array>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <print>
#include <variant>
#include <vector>

#include <windows.h>
#include <detours.h>

#include "win/env.h"

// https://github.com/microsoft/Detours/wiki/DetourCreateProcessWithDll#remarks
#pragma comment(linker, "/export:DetourFinishHelperProcess,@1,NONAME")

namespace catter::win {
namespace {
template <CharT char_t>
std::basic_string<char_t> get_app_name(const char_t* application_name, const char_t* command_line) {
    if(application_name == nullptr) {
        if(command_line == nullptr) {
            return {};
        }
        auto view = std::basic_string_view<char_t>(command_line);
        auto first_space = view.find_first_of(char_t(' '));
        return std::basic_string<char_t>(view.substr(0, first_space));
    } else {
        return std::basic_string<char_t>(application_name);
    }
}

template <CharT char_t>
DWORD FixGetEnvironmentVariable(const char_t* name, char_t* buffer, DWORD size) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetEnvironmentVariableA(name, buffer, size);
    } else {
        return GetEnvironmentVariableW(name, buffer, size);
    }
}

template <CharT char_t>
std::basic_string<char_t> get_proxy_path() {
    constexpr size_t buffer_size = 256;
    char_t buffer[buffer_size];

    auto len = FixGetEnvironmentVariable<char_t>(catter::win::ENV_VAR_PROXY_PATH<char_t>,
                                                 buffer,
                                                 buffer_size);
    if(len == 0) {
        return {};
    }

    if(len < buffer_size) {
        return std::basic_string<char_t>(buffer, len);
    }

    std::basic_string<char_t> path;
    path.resize(len);
    FixGetEnvironmentVariable<char_t>(catter::win::ENV_VAR_PROXY_PATH<char_t>, path.data(), len);
    path.pop_back();
    return path;
}

template <CharT char_t>
std::basic_string<char_t> get_ipc_id() {
    constexpr size_t buffer_size = 64;
    char_t buffer[buffer_size];

    auto len =
        FixGetEnvironmentVariable<char_t>(catter::win::ENV_VAR_IPC_ID<char_t>, buffer, buffer_size);
    if(len == 0) {
        return {};
    }

    if(len < buffer_size) {
        return std::basic_string<char_t>(buffer, len);
    }

    std::basic_string<char_t> id;
    id.resize(len);
    FixGetEnvironmentVariable<char_t>(catter::win::ENV_VAR_IPC_ID<char_t>, id.data(), len);
    id.pop_back();
    return id;
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

        auto converted_cmdline =
            std::format("{} -p {} --exec {} -- {}",
                        catter::win::get_proxy_path<char>(),
                        catter::win::get_ipc_id<char>(),
                        catter::win::get_app_name<char>(lpApplicationName, lpCommandLine),
                        std::string_view(lpCommandLine ? lpCommandLine : ""));

        return target(nullptr,
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

        auto converted_cmdline =
            std::format(L"{} -p {} --exec {} -- {}",
                        catter::win::get_proxy_path<wchar_t>(),
                        catter::win::get_ipc_id<wchar_t>(),
                        catter::win::get_app_name<wchar_t>(lpApplicationName, lpCommandLine),
                        std::wstring_view(lpCommandLine ? lpCommandLine : L""));

        return target(nullptr,
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

// TODO: implement these two
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
