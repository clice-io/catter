#include "resolver.h"

namespace catter::win::payload {

template <CharT char_t>
std::basic_string<char_t> Resolver<char_t>::resolve(std::basic_string_view<char_t> app_name) const {
    switch(m_mode) {
        case Mode::application_name:
            return hook::shared::resolver::resolve_application_name<char_t>(app_name);
        case Mode::command_line_token:
            return hook::shared::resolver::resolve_command_line_token<char_t>(app_name);
    }
    return {};
}

template <CharT char_t>
Resolver<char_t> create_app_name_resolver() {
    return Resolver<char_t>(Resolver<char_t>::Mode::application_name);
}

template <CharT char_t>
Resolver<char_t> create_command_line_resolver() {
    return Resolver<char_t>(Resolver<char_t>::Mode::command_line_token);
}

template class Resolver<char>;
template class Resolver<wchar_t>;

template Resolver<char> create_app_name_resolver();
template Resolver<wchar_t> create_app_name_resolver();
template Resolver<char> create_command_line_resolver();
template Resolver<wchar_t> create_command_line_resolver();

}  // namespace catter::win::payload
