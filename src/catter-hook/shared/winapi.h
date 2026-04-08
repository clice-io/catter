#pragma once
#ifdef CATTER_WINDOWS
#include <filesystem>
#include <type_traits>

#include <limits>
#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

namespace catter::win {

template <typename char_t>
concept CharT = std::is_same_v<char_t, char> || std::is_same_v<char_t, wchar_t>;

struct WinApiBufferDecision {
    bool done = false;
    size_t output_size = 0;
    size_t next_size = 0;
};

template <CharT char_t, typename size_t_winapi, typename api_call_t, typename decision_t>
std::basic_string<char_t> CallWinApiWithGrowingBuffer(size_t initial_size,
                                                      api_call_t&& call,
                                                      decision_t&& decide) {
    constexpr auto max_size = static_cast<size_t>((std::numeric_limits<size_t_winapi>::max)());
    size_t buffer_size = initial_size < 1 ? 1 : initial_size;
    if(buffer_size > max_size) {
        return {};
    }

    std::basic_string<char_t> buffer(buffer_size, char_t('\0'));
    while(true) {
        auto result = call(buffer.data(), static_cast<size_t_winapi>(buffer.size()));
        if(result == 0) {
            return {};
        }

        auto decision = decide(result, buffer.size());
        if(decision.done) {
            if(decision.output_size > buffer.size()) {
                return {};
            }
            buffer.resize(decision.output_size);
            return buffer;
        }

        auto next_size =
            decision.next_size > (buffer.size() + 1) ? decision.next_size : (buffer.size() + 1);
        if(next_size > max_size) {
            return {};
        }
        buffer.resize(next_size);
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
DWORD FixGetFullPathName(const char_t* file_name,
                         DWORD buffer_size,
                         char_t* buffer,
                         char_t** file_part) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetFullPathNameA(file_name, buffer_size, buffer, file_part);
    } else {
        return GetFullPathNameW(file_name, buffer_size, buffer, file_part);
    }
}

template <CharT char_t>
DWORD FixGetFileAttributes(const char_t* path) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetFileAttributesA(path);
    } else {
        return GetFileAttributesW(path);
    }
}

template <CharT char_t>
DWORD FixGetCurrentDirectory(DWORD size, char_t* buffer) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetCurrentDirectoryA(size, buffer);
    } else {
        return GetCurrentDirectoryW(size, buffer);
    }
}

template <CharT char_t>
DWORD FixGetModuleFileName(HMODULE module, char_t* buffer, DWORD size) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetModuleFileNameA(module, buffer, size);
    } else {
        return GetModuleFileNameW(module, buffer, size);
    }
}

template <CharT char_t>
DWORD FixSearchPath(const char_t* path,
                    const char_t* file_name,
                    const char_t* extension,
                    DWORD buffer_size,
                    char_t* buffer,
                    char_t** file_part) {
    if constexpr(std::is_same_v<char_t, char>) {
        return SearchPathA(path, file_name, extension, buffer_size, buffer, file_part);
    } else {
        return SearchPathW(path, file_name, extension, buffer_size, buffer, file_part);
    }
}

template <CharT char_t>
UINT FixGetSystemDirectory(char_t* buffer, UINT size) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetSystemDirectoryA(buffer, size);
    } else {
        return GetSystemDirectoryW(buffer, size);
    }
}

template <CharT char_t>
UINT FixGetWindowsDirectory(char_t* buffer, UINT size) {
    if constexpr(std::is_same_v<char_t, char>) {
        return GetWindowsDirectoryA(buffer, size);
    } else {
        return GetWindowsDirectoryW(buffer, size);
    }
}

template <CharT char_t>
std::basic_string<char_t> GetEnvironmentVariableDynamic(const char_t* name,
                                                        size_t initial_size = 256) {
    return CallWinApiWithGrowingBuffer<char_t, DWORD>(
        initial_size,
        [&](char_t* buffer, DWORD size) {
            return FixGetEnvironmentVariable<char_t>(name, buffer, size);
        },
        [](DWORD result, size_t buffer_size) -> WinApiBufferDecision {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision{.done = true,
                                            .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision{.next_size = static_cast<size_t>(result)};
        });
}

template <CharT char_t>
std::basic_string<char_t> GetCurrentDirectoryDynamic(size_t initial_size = MAX_PATH) {
    return CallWinApiWithGrowingBuffer<char_t, DWORD>(
        initial_size,
        [&](char_t* buffer, DWORD size) { return FixGetCurrentDirectory<char_t>(size, buffer); },
        [](DWORD result, size_t buffer_size) -> WinApiBufferDecision {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision{.done = true,
                                            .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision{.next_size = static_cast<size_t>(result) + 1};
        });
}

template <CharT char_t>
std::basic_string<char_t> GetModulePathDynamic(HMODULE module, size_t initial_size = MAX_PATH) {
    return CallWinApiWithGrowingBuffer<char_t, DWORD>(
        initial_size,
        [&](char_t* buffer, DWORD size) {
            return FixGetModuleFileName<char_t>(module, buffer, size);
        },
        [](DWORD result, size_t buffer_size) -> WinApiBufferDecision {
            auto written = static_cast<size_t>(result);
            if(written < buffer_size) {
                return WinApiBufferDecision{.done = true, .output_size = written};
            }
            return WinApiBufferDecision{.next_size = buffer_size * 2};
        });
}

template <CharT char_t>
std::basic_string<char_t> GetModuleDirectory(HMODULE module, size_t initial_size = MAX_PATH) {
    auto module_path = GetModulePathDynamic<char_t>(module, initial_size);
    if(module_path.empty()) {
        return {};
    }
    if constexpr(std::is_same_v<char_t, char>) {
        return std::filesystem::path(module_path).parent_path().string();
    } else {
        return std::filesystem::path(module_path).parent_path().wstring();
    }
}

template <CharT char_t>
std::basic_string<char_t> GetSystemDirectoryDynamic(size_t initial_size = MAX_PATH) {
    return CallWinApiWithGrowingBuffer<char_t, UINT>(
        initial_size,
        [&](char_t* buffer, UINT size) { return FixGetSystemDirectory<char_t>(buffer, size); },
        [](UINT result, size_t buffer_size) -> WinApiBufferDecision {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision{.done = true,
                                            .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision{.next_size = static_cast<size_t>(result) + 1};
        });
}

template <CharT char_t>
std::basic_string<char_t> GetWindowsDirectoryDynamic(size_t initial_size = MAX_PATH) {
    return CallWinApiWithGrowingBuffer<char_t, UINT>(
        initial_size,
        [&](char_t* buffer, UINT size) { return FixGetWindowsDirectory<char_t>(buffer, size); },
        [](UINT result, size_t buffer_size) -> WinApiBufferDecision {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision{.done = true,
                                            .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision{.next_size = static_cast<size_t>(result) + 1};
        });
}

template <CharT char_t>
std::basic_string<char_t> GetFullPathNameDynamic(std::basic_string_view<char_t> path,
                                                 size_t initial_size = MAX_PATH) {
    auto input = std::basic_string<char_t>(path);
    return CallWinApiWithGrowingBuffer<char_t, DWORD>(
        initial_size,
        [&](char_t* buffer, DWORD size) {
            return FixGetFullPathName<char_t>(input.c_str(), size, buffer, nullptr);
        },
        [](DWORD result, size_t buffer_size) -> WinApiBufferDecision {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision{.done = true,
                                            .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision{.next_size = static_cast<size_t>(result) + 1};
        });
}

template <CharT char_t>
std::basic_string<char_t> SearchPathDynamic(const char_t* path,
                                            std::basic_string_view<char_t> file_name,
                                            const char_t* extension = nullptr,
                                            size_t initial_size = MAX_PATH) {
    auto input = std::basic_string<char_t>(file_name);
    return CallWinApiWithGrowingBuffer<char_t, DWORD>(
        initial_size,
        [&](char_t* buffer, DWORD size) {
            return FixSearchPath<char_t>(path, input.c_str(), extension, size, buffer, nullptr);
        },
        [](DWORD result, size_t buffer_size) -> WinApiBufferDecision {
            if(static_cast<size_t>(result) < buffer_size) {
                return WinApiBufferDecision{.done = true,
                                            .output_size = static_cast<size_t>(result)};
            }
            return WinApiBufferDecision{.next_size = static_cast<size_t>(result) + 1};
        });
}
}  // namespace catter::win::payload

#endif