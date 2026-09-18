#pragma once
#include <filesystem>

#include "winapi/win32.h"

namespace catter::proxy::hook {
bool try_inject(HANDLE hProcess, const std::filesystem::path& dll_path);
}
