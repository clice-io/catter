#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <thread>
#include <print>

#include "common.h"


#include <windows.h>
#include <detours.h>



// https://github.com/microsoft/Detours/wiki/DetourCreateProcessWithDll#remarks
#pragma comment(linker, "/export:DetourFinishHelperProcess,@1,NONAME")

// Use anonymous namespace to avoid exporting symbols
namespace {

class unique_file{
public:
    unique_file(){
        std::error_code ec;
        std::filesystem::create_directory(catter::capture_root, ec);
        if (ec) {
            std::println("Failed to create capture root directory: {}: {}", catter::capture_root, ec.message());
            return;
        }

        auto unique_id = std::this_thread::get_id();
        auto path = std::format("{}/{}", catter::capture_root, std::to_string(std::hash<std::thread::id>()(unique_id)));
        this->ofs.open(path, std::ios::out | std::ios::app);

        if (!this->ofs.is_open()) {
            std::println("Failed to open output file: {}", path);
            std::println("Error: {}", std::error_code(errno, std::generic_category()).message());
        }
    }

    template<typename... args_t>
    void writeln(std::format_string<args_t...> fmt, args_t&&... args) {
        std::lock_guard lock(this->mutex);
        if (this->ofs.is_open()) {
            this->ofs << std::format(fmt, std::forward<args_t>(args)...) << std::endl;
        }
    }

    template<typename... args_t>
    void wwriteln(std::wformat_string<args_t...> fmt, args_t&&... args) {
        std::error_code ec;
        auto message = catter::wstring_to_utf8(std::format(fmt, std::forward<args_t>(args)...), ec);
        if (ec) {
            this->writeln("Failed to convert wide string to UTF-8: {}", ec.message());
        } else {
            this->writeln("{}", std::move(message));
        }
    }


    ~unique_file(){
        this->ofs.close();
    }

private:
    std::ofstream ofs;
    std::mutex     mutex;
};

unique_file& output_file(){
    static unique_file instance;
    return instance;
}



namespace hook {

    struct CreateProcessA {
        static inline decltype(::CreateProcessA)* target = ::CreateProcessA;
        static BOOL WINAPI detour(
            LPCSTR lpApplicationName,
            LPSTR lpCommandLine,
            LPSECURITY_ATTRIBUTES lpProcessAttributes,
            LPSECURITY_ATTRIBUTES lpThreadAttributes,
            BOOL bInheritHandles,
            DWORD dwCreationFlags,
            LPVOID lpEnvironment,
            LPCSTR lpCurrentDirectory,
            LPSTARTUPINFOA lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ){
            output_file().writeln("{} {}", 
                lpApplicationName ? lpApplicationName : "",
                lpCommandLine ? lpCommandLine : ""
            );

            return DetourCreateProcessWithDllExA(
                lpApplicationName,
                lpCommandLine,
                lpProcessAttributes,
                lpThreadAttributes,
                bInheritHandles,
                dwCreationFlags,
                lpEnvironment,
                lpCurrentDirectory,
                lpStartupInfo,
                lpProcessInformation,
                catter::hook_dll,
                target
            );
        }
    };

    struct CreateProcessW {
        static inline decltype(::CreateProcessW)* target = ::CreateProcessW;
        static BOOL WINAPI detour(
            LPCWSTR lpApplicationName,
            LPWSTR lpCommandLine,
            LPSECURITY_ATTRIBUTES lpProcessAttributes,
            LPSECURITY_ATTRIBUTES lpThreadAttributes,
            BOOL bInheritHandles,
            DWORD dwCreationFlags,
            LPVOID lpEnvironment,
            LPCWSTR lpCurrentDirectory,
            LPSTARTUPINFOW lpStartupInfo,
            LPPROCESS_INFORMATION lpProcessInformation
        ){
            output_file().wwriteln(L"{} {}", 
                lpApplicationName ? lpApplicationName : L"", 
                lpCommandLine ? lpCommandLine : L""
            );
            return DetourCreateProcessWithDllExW(
                lpApplicationName,
                lpCommandLine,
                lpProcessAttributes,
                lpThreadAttributes,
                bInheritHandles,
                dwCreationFlags,
                lpEnvironment,
                lpCurrentDirectory,
                lpStartupInfo,
                lpProcessInformation,
                catter::hook_dll,
                target
            );
        }
    };

    struct detour_meta {
        std::string_view    name;
        void**              target;
        void*               detour;
    };

    template <typename... args_t>
    std::vector<detour_meta> collect_fn() noexcept {
        return {
            { 
                meta::type_name<args_t>(), 
                reinterpret_cast<void**>(&args_t::target), 
                reinterpret_cast<void*>(args_t::detour) 
            } ...
        };
    }

    auto& fn() noexcept {
        static auto instance = collect_fn<
            CreateProcessA,
            CreateProcessW
        >();
        return instance;
    }

    void attach() noexcept {
        for (auto& m : hook::fn()){
            // println"Attaching hook for `{}`", m.name);
            DetourAttach(m.target, m.detour);
        }
    }

    void detach() noexcept {
        for (auto& m : hook::fn()){
            DetourDetach(m.target, m.detour);
        }
    }
}

};


BOOL WINAPI DllMain (HINSTANCE hinst, DWORD dwReason, LPVOID reserved) {
    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    if (dwReason == DLL_PROCESS_ATTACH) {
        DetourRestoreAfterWith();

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        hook::attach();

        DetourTransactionCommit();
    } else if (dwReason == DLL_PROCESS_DETACH) {
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        hook::detach();

        DetourTransactionCommit();
    }
    return TRUE;
}
