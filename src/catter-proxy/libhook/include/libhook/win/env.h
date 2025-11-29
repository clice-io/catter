#pragma once

#include <filesystem>
#include <vector>
#include <windows.h>

namespace catter::win {

// Anonymous namespace for internal linkage
// to avoid symbol conflicts.
namespace {
constexpr static char exe_name[] = "catter-proxy.exe";
constexpr static char dll_name[] = "catter-hook64.dll";
}  // namespace
}  // namespace catter::win
