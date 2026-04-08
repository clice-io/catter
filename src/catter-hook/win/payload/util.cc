#include <string>
#include <string_view>

#include "win/env.h"

#include "shared/resolver.h"
#include "util.h"

namespace catter::win::payload {

namespace {
template <CharT char_t>
std::basic_string<char_t> resolve_abspath_impl(const char_t* application_name,
                                               const char_t* command_line) {
    auto resolver = hook::shared::WindowsResolver<char_t>::from_current_process();
    std::basic_string<char_t> raw_app_name;

    if(application_name != nullptr && application_name[0] != char_t('\0')) {
        raw_app_name.assign(application_name);
        if(auto resolved = resolver.resolve_application_name(raw_app_name); resolved.has_value()) {
            return std::move(*resolved);
        }
        return raw_app_name;
    }

    raw_app_name = hook::shared::extract_command_line_token<char_t>(
        command_line == nullptr ? std::basic_string_view<char_t>{}
                                : std::basic_string_view<char_t>{command_line});
    if(raw_app_name.empty()) {
        return {};
    }

    if(auto resolved = resolver.resolve_command_line_token(raw_app_name); resolved.has_value()) {
        return std::move(*resolved);
    }
    return raw_app_name;
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
