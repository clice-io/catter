#include <algorithm>
#include <string>
#include <string_view>

#include "win/env.h"

#include "resolver.h"
#include "util.h"

namespace catter::win::payload {

namespace {

template <CharT char_t>
std::basic_string<char_t> extract_first_token(std::basic_string_view<char_t> command_line) {
    constexpr char_t quote_char = char_t('"');
    constexpr char_t space_char = char_t(' ');

    auto trimmed = [](std::basic_string_view<char_t> value) {
        auto first_non_space =
            std::find_if_not(value.begin(), value.end(), [](char_t c) { return c == space_char; });
        return std::basic_string<char_t>(first_non_space, value.end());
    }(command_line);

    std::basic_string_view<char_t> view(trimmed);
    if(view.empty()) {
        return {};
    }

    if(view.front() == quote_char) {
        auto closing_quote = view.find(quote_char, 1);
        if(closing_quote == std::basic_string_view<char_t>::npos) {
            return std::basic_string<char_t>(view.substr(1));
        }
        return std::basic_string<char_t>(view.substr(1, closing_quote - 1));
    }

    auto first_space =
        std::find_if(view.begin(), view.end(), [](char_t c) { return c == space_char; });
    return std::basic_string<char_t>(view.begin(), first_space);
}

template <CharT char_t>
std::basic_string<char_t> resolve_abspath_impl(const char_t* application_name,
                                               const char_t* command_line) {
    std::basic_string<char_t> raw_app_name;

    // CreateProcess takes lpApplicationName first; when absent, the executable
    // is parsed from the first token in lpCommandLine.
    if(application_name != nullptr && application_name[0] != char_t('\0')) {
        raw_app_name.assign(application_name);
        return create_app_name_resolver<char_t>().resolve(raw_app_name);
    }

    raw_app_name = extract_first_token<char_t>(command_line == nullptr
                                                   ? std::basic_string_view<char_t>{}
                                                   : std::basic_string_view<char_t>{command_line});
    if(raw_app_name.empty()) {
        return {};
    }

    return create_command_line_resolver<char_t>().resolve(raw_app_name);
}

}  // namespace

template <CharT char_t>
std::basic_string<char_t> resolve_abspath(const char_t* application_name,
                                          const char_t* command_line) {
    return resolve_abspath_impl<char_t>(application_name, command_line);
}

template <CharT char_t>
std::basic_string<char_t> get_proxy_path() {
    return GetEnvironmentVariableDynamic<char_t>(catter::win::ENV_VAR_PROXY_PATH<char_t>, 256);
}

template <CharT char_t>
std::basic_string<char_t> get_ipc_id() {
    return GetEnvironmentVariableDynamic<char_t>(catter::win::ENV_VAR_IPC_ID<char_t>, 64);
}

template std::basic_string<char> resolve_abspath(const char* application_name,
                                                 const char* command_line);
template std::basic_string<wchar_t> resolve_abspath(const wchar_t* application_name,
                                                    const wchar_t* command_line);
template std::basic_string<char> get_proxy_path();
template std::basic_string<wchar_t> get_proxy_path();
template std::basic_string<char> get_ipc_id();
template std::basic_string<wchar_t> get_ipc_id();

}  // namespace catter::win::payload
