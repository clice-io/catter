#include "resolver.h"

namespace catter::win::payload {
template <hook::shared::WindowsChar char_t>
Resolver<char_t>::Resolver(hook::shared::WindowsResolver<char_t> resolver, Mode mode) :
    resolver_(std::move(resolver)), mode_(mode) {}

template <hook::shared::WindowsChar char_t>
std::basic_string<char_t> Resolver<char_t>::resolve(std::basic_string_view<char_t> token) const {
    auto resolved = mode_ == Mode::application_name ? resolver_.resolve_application_name(token)
                                                    : resolver_.resolve_command_line_token(token);
    if(resolved.has_value()) {
        return std::move(*resolved);
    }
    return std::basic_string<char_t>(token);
}

template <hook::shared::WindowsChar char_t>
Resolver<char_t> create_app_name_resolver() {
    return Resolver<char_t>(hook::shared::WindowsResolver<char_t>::from_current_process(),
                            Resolver<char_t>::Mode::application_name);
}

template <hook::shared::WindowsChar char_t>
Resolver<char_t> create_command_line_resolver() {
    return Resolver<char_t>(hook::shared::WindowsResolver<char_t>::from_current_process(),
                            Resolver<char_t>::Mode::command_line_token);
}

template class Resolver<char>;
template class Resolver<wchar_t>;

template Resolver<char> create_app_name_resolver();
template Resolver<wchar_t> create_app_name_resolver();
template Resolver<char> create_command_line_resolver();
template Resolver<wchar_t> create_command_line_resolver();

}  // namespace catter::win::payload
