
#pragma once
#include <chrono>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// clang-format off
// Windows SDK headers are order-sensitive: <windows.h> defines the macros and
// types (HANDLE, DWORD, BOOL, HMODULE, ...) that the others depend on, so it
// must come first. Do not let clang-format alphabetize this block.
#include <windows.h>
#include <Psapi.h>
#include <libloaderapi.h>
#include <minwindef.h>
// clang-format on

namespace catter::win {
class Handle {
public:
    Handle(HANDLE handle = nullptr) : h(handle) {}

    ~Handle() {
        close();
    }

    Handle(const Handle&) = delete;
    Handle& operator= (const Handle&) = delete;

    Handle(Handle&& other) noexcept : h(std::exchange(other.h, nullptr)) {}

    Handle& operator= (Handle&& other) noexcept {
        if(this != &other) {
            close();
            h = std::exchange(other.h, nullptr);
        }
        return *this;
    }

    explicit operator bool() const noexcept {
        return this->valid();
    }

    bool valid() const noexcept {
        return h != nullptr && h != INVALID_HANDLE_VALUE;
    }

    HANDLE get() const noexcept {
        return h;
    }

    HANDLE release() noexcept {
        return std::exchange(h, nullptr);
    }

    void close() noexcept {
        auto old = std::exchange(h, nullptr);
        if(old != nullptr && old != INVALID_HANDLE_VALUE) {
            CloseHandle(old);
        }
    }

private:
    HANDLE h;
};

class RemoteMemory {
public:
    RemoteMemory() = default;

    RemoteMemory(HANDLE hProcess,
                 LPVOID lpAddress,
                 SIZE_T dwSize,
                 DWORD flAllocationType,
                 DWORD flProtect) : process(hProcess) {
        space = VirtualAllocEx(hProcess, lpAddress, dwSize, flAllocationType, flProtect);
        if(!space)
            throw std::runtime_error("VirtualAllocEx failed");
    }

    RemoteMemory(const RemoteMemory&) = delete;

    RemoteMemory(RemoteMemory&& other) noexcept :
        process(std::exchange(other.process, nullptr)), space(std::exchange(other.space, nullptr)) {
    }

    RemoteMemory& operator= (const RemoteMemory&) = delete;

    RemoteMemory& operator= (RemoteMemory&& other) noexcept {
        if(this != &other) {
            this->free();
            this->process = std::exchange(other.process, nullptr);
            this->space = std::exchange(other.space, nullptr);
        }
        return *this;
    }

    void free() {
        if(this->space && this->process) {
            VirtualFreeEx(process, space, 0, MEM_RELEASE);
            space = nullptr;
        }
    }

    ~RemoteMemory() {
        this->free();
    }

    LPVOID get() const {
        return space;
    }

private:
    HANDLE process = nullptr;
    LPVOID space = nullptr;
};

template <typename F>
    requires std::is_function_v<F>
F* get_function_from_ntdll(const char* name) {
    HMODULE hNtDllModule = GetModuleHandleA("ntdll.dll");
    if(hNtDllModule == NULL) {
        throw std::system_error(
            GetLastError(),
            std::system_category(),
            std::format("Failed to get handle of ntdll.dll when looking for function {}", name));
    }

    auto* fn = reinterpret_cast<F*>(GetProcAddress(hNtDllModule, name));
    if(fn == nullptr) {
        throw std::system_error(ERROR_PROC_NOT_FOUND,
                                std::system_category(),
                                std::format("Failed to find {} in ntdll.dll", name));
    }
    return fn;
}

inline std::error_code wait_for_object(HANDLE handle,
                                       std::chrono::milliseconds ms = std::chrono::milliseconds{
                                           INFINITE}) {

    switch(WaitForSingleObject(handle, static_cast<DWORD>(ms.count()))) {
        case WAIT_OBJECT_0: return {};

        case WAIT_TIMEOUT: return std::make_error_code(std::errc::timed_out);
        case WAIT_FAILED:
        default: return std::error_code(GetLastError(), std::system_category());
    }
}

inline std::string quote_win32_arg(std::string_view arg) noexcept {
    // No quoting needed if it's empty or has no special characters.
    if(arg.empty() || arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(arg);
    }

    std::string quoted_arg;
    quoted_arg.push_back('"');

    for(auto it = arg.begin();; ++it) {
        int num_backslashes = 0;
        while(it != arg.end() && *it == '\\') {
            ++it;
            ++num_backslashes;
        }

        if(it == arg.end()) {
            // End of string; append backslashes and a closing quote.
            quoted_arg.append(num_backslashes * 2, '\\');
            break;
        }

        if(*it == '"') {
            // Escape all backslashes and the following double quote.
            quoted_arg.append(num_backslashes * 2 + 1, '\\');
            quoted_arg.push_back(*it);
        } else {
            // Backslashes aren't special here.
            quoted_arg.append(num_backslashes, '\\');
            quoted_arg.push_back(*it);
        }
    }
    quoted_arg.push_back('"');
    return quoted_arg;
}
}  // namespace catter::win
