#pragma once

#include <string>
#include <string_view>
#include <type_traits>

#include "winapi/win32.h"

namespace catter::win::payload {

template <CharT char_t>
std::basic_string<char_t> resolve_abspath(const char_t* application_name,
                                          const char_t* command_line);
template <CharT char_t>
std::basic_string<char_t> get_proxy_path();

template <CharT char_t>
std::basic_string<char_t> get_ipc_id();

template <CharT char_t>
std::basic_string<char_t> build_proxy_command(std::basic_string_view<char_t> proxy_path,
                                              std::basic_string_view<char_t> ipc_id,
                                              const char_t* application_name,
                                              const char_t* command_line);

}  // namespace catter::win::payload
