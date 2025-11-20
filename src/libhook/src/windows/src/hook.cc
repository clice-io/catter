#include <array>
#include <cstddef>
#include <format>
#include <string_view>
#include <print>

#include <windows.h>
#include <detours.h>

#include "../env.h"

// https://github.com/microsoft/Detours/wiki/DetourCreateProcessWithDll#remarks
#pragma comment(linker, "/export:DetourFinishHelperProcess,@1,NONAME")

namespace meta {
template <typename T>
consteval std::string_view type_name() {
    std::string_view name =
#if defined(__clang__) || defined(__GNUC__)
        __PRETTY_FUNCTION__;  // Clang / GCC
#elif defined(_MSC_VER)
        __FUNCSIG__;  // MSVC
#else
        static_assert(false, "Unsupported compiler");
#endif

#if defined(__clang__)
    constexpr std::string_view prefix = "std::string_view meta::type_name() [T = ";
    constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
    constexpr std::string_view prefix = "consteval std::string_view meta::type_name() [with T = ";
    constexpr std::string_view suffix = "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
    constexpr std::string_view prefix =
        "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl meta::type_name<";
    constexpr std::string_view suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

}  // namespace meta

namespace catter::win {
namespace {

HINSTANCE& dll_instance() {
    static HINSTANCE instance = nullptr;
    return instance;
}

}  // namespace

}  // namespace catter::win

// Use anonymous namespace to avoid exporting symbols
namespace {

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
