#pragma once

#include <string>
#include <string_view>

#include "shared/resolver.h"

namespace catter::win::payload {

template <hook::shared::WindowsChar char_t>
class Resolver {
public:
    enum class Mode {
        application_name,
        command_line_token,
    };

    Resolver(hook::shared::WindowsResolver<char_t> resolver, Mode mode);

    std::basic_string<char_t> resolve(std::basic_string_view<char_t> token) const;

private:
    hook::shared::WindowsResolver<char_t> resolver_;
    Mode mode_;
};

template <hook::shared::WindowsChar char_t>
Resolver<char_t> create_app_name_resolver();

template <hook::shared::WindowsChar char_t>
Resolver<char_t> create_command_line_resolver();

extern template class Resolver<char>;
extern template class Resolver<wchar_t>;

}  // namespace catter::win::payload
