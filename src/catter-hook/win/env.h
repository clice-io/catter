#pragma once

#include <filesystem>
#include <vector>

#include "shared/winapi.h"

namespace catter::win {

constexpr static char EXE_NAME[] = "catter-proxy.exe";
constexpr static char DLL_NAME[] = "catter-hook-win64.dll";

template <CharT char_t>
constexpr char_t ENV_VAR_IPC_ID[] = {};

template <>
inline constexpr char ENV_VAR_IPC_ID<char>[] = "CATTER_IPC_ID";

template <>
inline constexpr wchar_t ENV_VAR_IPC_ID<wchar_t>[] = L"CATTER_IPC_ID";

template <CharT char_t>
constexpr char_t ENV_VAR_PROXY_PATH[] = {};

template <>
inline constexpr char ENV_VAR_PROXY_PATH<char>[] = "CATTER_PROXY_PATH";

template <>
inline constexpr wchar_t ENV_VAR_PROXY_PATH<wchar_t>[] = L"CATTER_PROXY_PATH";

using ipc_id_t = int64_t;

}  // namespace catter::win
