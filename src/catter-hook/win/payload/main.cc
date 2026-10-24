#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <MinHook.h>

#include "win/payload/util.h"

// Use anonymous namespace to avoid exporting symbols
namespace {

namespace minhook {

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

        auto converted_cmdline = catter::win::payload::build_proxy_command<wchar_t>(
            catter::win::payload::get_proxy_path<wchar_t>(),
            catter::win::payload::get_ipc_id<wchar_t>(),
            catter::win::payload::get_ipc_pipe<wchar_t>(),
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
    constexpr static char name[] = "CreateProcessA";
    inline static decltype(::CreateProcessA)* target = &::CreateProcessA;
    inline static decltype(::CreateProcessA)* original = nullptr;

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
        if(MH_CreateHook(m.target, m.detour_func, m.original_ptr) != MH_OK) {
            // TODO: logging hook creation failed for m.name
        }
    }
}

void detach() noexcept {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}
}  // namespace minhook
}  // namespace

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD dwReason, [[maybe_unused]] LPVOID reserved) {
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
