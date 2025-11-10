#pragma once
#include <cstddef>
#include <windows.h>

namespace catter::win {
constexpr static char hook_dll_name[] = "catter-hook64.dll";
constexpr static char capture_root[] = "catter-captured";

struct path {
    size_t length{};
    char data[MAX_PATH]{};
};

static path hook_dll_path{};

}  // namespace catter::win
