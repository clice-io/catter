#pragma once
#ifdef CATTER_WINDOWS
#include <chrono>
#include <string>
#include <string_view>
#include <type_traits>

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

template <typename char_t>
concept CharT = std::is_same_v<char_t, char> || std::is_same_v<char_t, wchar_t>;

template <CharT char_t>
DWORD FixGetEnvironmentVariable(const char_t* name, char_t* buffer, DWORD size);

template <CharT char_t>
DWORD FixGetFullPathName(const char_t* file_name,
                         DWORD buffer_size,
                         char_t* buffer,
                         char_t** file_part);

template <CharT char_t>
DWORD FixGetFileAttributes(const char_t* path);

template <CharT char_t>
DWORD FixGetCurrentDirectory(DWORD size, char_t* buffer);

template <CharT char_t>
DWORD FixGetModuleFileName(HMODULE module, char_t* buffer, DWORD size);

template <CharT char_t>
DWORD FixSearchPath(const char_t* path,
                    const char_t* file_name,
                    const char_t* extension,
                    DWORD buffer_size,
                    char_t* buffer,
                    char_t** file_part);

template <CharT char_t>
UINT FixGetSystemDirectory(char_t* buffer, UINT size);

template <CharT char_t>
UINT FixGetWindowsDirectory(char_t* buffer, UINT size);

template <CharT char_t>
std::basic_string<char_t> GetEnvironmentVariableDynamic(const char_t* name,
                                                        size_t initial_size = 256);
template <CharT char_t>
std::basic_string<char_t> GetCurrentDirectoryDynamic(size_t initial_size = MAX_PATH);

template <CharT char_t>
std::basic_string<char_t> GetModulePathDynamic(HMODULE module, size_t initial_size = MAX_PATH);

template <CharT char_t>
std::basic_string<char_t> GetModuleDirectory(HMODULE module, size_t initial_size = MAX_PATH);

template <CharT char_t>
std::basic_string<char_t> GetSystemDirectoryDynamic(size_t initial_size = MAX_PATH);

template <CharT char_t>
std::basic_string<char_t> GetWindowsDirectoryDynamic(size_t initial_size = MAX_PATH);

template <CharT char_t>
std::basic_string<char_t> GetFullPathNameDynamic(std::basic_string_view<char_t> path,
                                                 size_t initial_size = MAX_PATH);

template <CharT char_t>
std::basic_string<char_t> SearchPathDynamic(const char_t* path,
                                            std::basic_string_view<char_t> file_name,
                                            const char_t* extension = nullptr,
                                            size_t initial_size = MAX_PATH);

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

std::error_code wait_for_object(HANDLE handle,
                                std::chrono::milliseconds ms = std::chrono::milliseconds{
                                    INFINITE}) noexcept;

std::string quote_win32_arg(std::string_view arg) noexcept;
}  // namespace catter::win

#endif
