#pragma once

#include <string>
#include <string_view>

namespace catter::win::payload {

template <typename char_t>
concept CharT = std::is_same_v<char_t, char> || std::is_same_v<char_t, wchar_t>;

template <CharT char_t>
std::basic_string<char_t> resolve_abspath(const char_t* application_name,
                                          const char_t* command_line);
template <CharT char_t>
std::basic_string<char_t> get_proxy_path();

template <CharT char_t>
std::basic_string<char_t> get_ipc_id();

}  // namespace catter::win::payload
