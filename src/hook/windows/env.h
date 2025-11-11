#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <tuple>
#include <windows.h>
namespace catter::win {
constexpr static char hook_dll_name[] = "catter-hook64.dll";
constexpr static char capture_root[] = "catter-captured";

inline auto path(HMODULE h = nullptr) {
    size_t max_size = MAX_PATH;
    size_t length = 0;
    std::unique_ptr<char[]> data;
    do {
        max_size *= 2;
        data = std::make_unique<char[]>(max_size);
        length = GetModuleFileNameA(h, data.get(), max_size);
    } while (length == max_size && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    return std::make_tuple(
        length,
        std::move(data)
    );
}


}  // namespace catter::win
