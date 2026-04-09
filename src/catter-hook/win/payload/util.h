#pragma once

#include <string>
#include <string_view>

#include "shared/winapi.h"

namespace catter::win::payload {

template <CharT char_t>
std::basic_string<char_t> resolve_abspath(const char_t* application_name,
                                          const char_t* command_line);
template <CharT char_t>
std::basic_string<char_t> get_proxy_path();

template <CharT char_t>
std::basic_string<char_t> get_ipc_id();

}  // namespace catter::win::payload
