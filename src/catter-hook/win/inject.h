#pragma once
#include <filesystem>
#include <windows.h>

namespace catter::proxy::hook {
bool try_inject(HANDLE hProcess, const std::filesystem::path& dll_path);
}
