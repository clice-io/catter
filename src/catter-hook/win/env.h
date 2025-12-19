#pragma once

#include <filesystem>
#include <vector>
#include <windows.h>

namespace catter::win {

// Anonymous namespace for internal linkage
// to avoid symbol conflicts.
namespace {
constexpr static char EXE_NAME[] = "catter-proxy.exe";
constexpr static char DLL_NAME[] = "catter-hook-win64.dll";

template <typename char_t>
concept CharT = std::is_same_v<char_t, char> || std::is_same_v<char_t, wchar_t>;

template <CharT char_t>
constexpr char_t ENV_VAR_RPC_ID[] = {};

template <>
constexpr char ENV_VAR_RPC_ID<char>[] = "CATTER_RPC_ID";

template <>
constexpr wchar_t ENV_VAR_RPC_ID<wchar_t>[] = L"CATTER_RPC_ID";

using rpc_id_t = int64_t;

}  // namespace
}  // namespace catter::win
