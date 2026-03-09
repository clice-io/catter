#include <array>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <print>
#include <variant>
#include <vector>

#include <MinHook.h>
#include <windows.h>

#include "win/env.h"

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

namespace minhook {

struct CreateProcessA {
    constexpr static char name[] = "CreateProcessA";
    inline static decltype(::CreateProcessA)* target = &::CreateProcessA;
    inline static decltype(::CreateProcessA)* original = nullptr;

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

struct CreateProcessW {
    constexpr static char name[] = "CreateProcessW";
    inline static decltype(::CreateProcessW)* target = &::CreateProcessW;
    inline static decltype(::CreateProcessW)* original = nullptr;

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

struct CreateProcessAsUserA {
    constexpr static char name[] = "CreateProcessAsUserA";
    inline static decltype(::CreateProcessAsUserA)* target = &::CreateProcessAsUserA;
    inline static decltype(::CreateProcessAsUserA)* original = nullptr;

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

        return original(hToken,
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
    constexpr static char name[] = "CreateProcessAsUserW";
    inline static decltype(::CreateProcessAsUserW)* target = &::CreateProcessAsUserW;
    inline static decltype(::CreateProcessAsUserW)* original = nullptr;

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

        return original(hToken,
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

struct minhook_meta {
    std::string_view name;
    void* target;
    void* detour_func;
    void** original_ptr;
};

template <typename... args_t>
auto collect_fn() noexcept {
    return std::array<minhook_meta, sizeof...(args_t)>{
        minhook_meta{args_t::name,
                     (void*)args_t::target,
                     (void*)args_t::detour,
                     (void**)(&args_t::original)}
        ...
    };
}

auto& fn() noexcept {
    static auto instance =
        collect_fn<CreateProcessA, CreateProcessW, CreateProcessAsUserA, CreateProcessAsUserW>();
    return instance;
}

void attach() noexcept {
    for(auto& m: fn()) {
        MH_CreateHook(m.target, m.detour_func, m.original_ptr);
    }
}

void detach() noexcept {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
}  // namespace minhook
}  // namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
    switch(dwReason) {
        case DLL_PROCESS_ATTACH:
            // avoid the DLL_THREAD_ATTACH and DLL_THREAD_DETACH notifications to reduce overhead
            DisableThreadLibraryCalls(hinst);

            if(MH_Initialize() != MH_OK) {
                return FALSE;
            }
            // trampoline hooking using MinHook
            minhook::attach();
            if(MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
                return FALSE;
            }
            break;

        case DLL_PROCESS_DETACH: minhook::detach(); break;
    }
    return TRUE;
}
