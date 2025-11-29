#include <array>
#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <print>
#include <vector>

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

std::filesystem::path current_path() {
    std::vector<char> data;
    data.resize(MAX_PATH);

    while(true) {
        if(GetModuleFileNameA(dll_instance(), data.data(), data.size()) == data.size() &&
           GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            data.resize(data.size() * 2);
        } else {
            break;
        }
    }

    return std::filesystem::path(data.data()).parent_path();
}

std::filesystem::path get_catter_exe_path() {
    return current_path() / EXE_NAME;
}

template <CharT char_t>
std::basic_string<char_t> concat_cmdline(const char_t* application_name,
                                         const char_t* command_line) {
    if(application_name == nullptr) {
        return std::basic_string<char_t>(command_line);
    } else {
        if constexpr(std::is_same_v<char_t, char>) {
            return std::format("\"{}\" {}", application_name, command_line);
        } else {
            return std::format(L"\"{}\" {}", application_name, command_line);
        }
    }
}

template <CharT char_t>
std::basic_string<char_t> get_rpc_id() {
    constexpr size_t buffer_size = 64;
    char_t buffer[buffer_size];

    if constexpr(std::is_same_v<char_t, char>) {
        if(GetEnvironmentVariableA(catter::win::ENV_VAR_RPC_ID<char_t>, buffer, buffer_size)) {
            return std::basic_string<char_t>(buffer);
        } else {
            // TODO: log
        }

    } else {
        if(GetEnvironmentVariableW(catter::win::ENV_VAR_RPC_ID<char_t>, buffer, buffer_size)) {
            return std::basic_string<char_t>(buffer);
        } else {
            // TODO: log
        }
    }
    return buffer;
}

template <CharT char_t>
bool is_key_match(std::basic_string_view<char_t> entry, std::basic_string_view<char_t> target_key) {

    if(entry.length() <= target_key.length()) {
        return false;
    }

    if(entry[target_key.length()] != '=') {
        return false;
    }

    return entry.substr(0, target_key.length()) == target_key;
}

template <CharT char_t>
std::vector<char_t> fix_env_block(char_t* env_block,
                                  const std::basic_string<char_t>& rpc_id_entry) {
    constexpr char_t char_zero = []() {
        if constexpr(std::is_same_v<char_t, char>) {
            return '\0';
        } else {
            return L'\0';
        }
    }();

    std::vector<char_t> result;

    bool found = false;
    for(auto current = env_block; *current != char_zero;) {
        std::basic_string_view<char_t> sv(current);
        std::span<const char_t> span(sv.data(), sv.size() + 1);

        result.append_range(span);
        std::advance(current, span.size());
        if(is_key_match<char_t>(sv, catter::win::ENV_VAR_RPC_ID<char_t>)) {
            found = true;
        }
    }

    if(!found) {
        std::span<const char_t> span(rpc_id_entry.c_str(), rpc_id_entry.size() + 1);
        result.append_range(span);
    }

    result.push_back(char_zero);  // Double null termination
    return result;
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
        std::vector<char> fixed_env_blockA;
        std::vector<wchar_t> fixed_env_blockW;

        void* final_env = lpEnvironment;
        if(final_env) {
            if(dwCreationFlags & CREATE_UNICODE_ENVIRONMENT) {
                auto rpc_id_entry = std::format(L"{}={}",
                                                catter::win::ENV_VAR_RPC_ID<wchar_t>,
                                                catter::win::get_rpc_id<wchar_t>());
                fixed_env_blockW =
                    catter::win::fix_env_block<wchar_t>((wchar_t*)lpEnvironment, rpc_id_entry);
                final_env = fixed_env_blockW.data();

            } else {
                auto rpc_id_entry = std::format("{}={}",
                                                catter::win::ENV_VAR_RPC_ID<char>,
                                                catter::win::get_rpc_id<char>());
                fixed_env_blockA =
                    catter::win::fix_env_block<char>((char*)lpEnvironment, rpc_id_entry);
                final_env = fixed_env_blockA.data();
            }
        }

        auto converted_cmdline =
            std::format("{} -p {} -- {}",
                        catter::win::get_catter_exe_path().string(),
                        catter::win::get_rpc_id<char>(),
                        catter::win::concat_cmdline<char>(lpApplicationName, lpCommandLine));

        return target(nullptr,
                      converted_cmdline.data(),
                      lpProcessAttributes,
                      lpThreadAttributes,
                      bInheritHandles,
                      dwCreationFlags,
                      final_env,
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

        std::vector<char> fixed_env_blockA;
        std::vector<wchar_t> fixed_env_blockW;

        void* final_env = lpEnvironment;
        if(final_env) {
            if(dwCreationFlags & CREATE_UNICODE_ENVIRONMENT) {
                auto rpc_id_entry = std::format(L"{}={}",
                                                catter::win::ENV_VAR_RPC_ID<wchar_t>,
                                                catter::win::get_rpc_id<wchar_t>());
                fixed_env_blockW =
                    catter::win::fix_env_block<wchar_t>((wchar_t*)lpEnvironment, rpc_id_entry);
                final_env = fixed_env_blockW.data();

            } else {
                auto rpc_id_entry = std::format("{}={}",
                                                catter::win::ENV_VAR_RPC_ID<char>,
                                                catter::win::get_rpc_id<char>());
                fixed_env_blockA =
                    catter::win::fix_env_block<char>((char*)lpEnvironment, rpc_id_entry);
                final_env = fixed_env_blockA.data();
            }
        }

        auto converted_cmdline =
            std::format(L"{} -p {} -- {}",
                        catter::win::get_catter_exe_path().wstring(),
                        catter::win::get_rpc_id<wchar_t>(),
                        catter::win::concat_cmdline<wchar_t>(lpApplicationName, lpCommandLine));

        return target(nullptr,
                      converted_cmdline.data(),
                      lpProcessAttributes,
                      lpThreadAttributes,
                      bInheritHandles,
                      dwCreationFlags,
                      final_env,
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
