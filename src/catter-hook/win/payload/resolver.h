#pragma once

#include <algorithm>
#include <string>
#include <string_view>

#include "winapi.h"
#include "shared/resolver.h"

namespace catter::win::payload {

template <CharT char_t>
class Resolver {
public:
    enum class Mode {
        application_name,
        command_line_token,
    };

    explicit Resolver(Mode mode) : m_mode(mode) {}

    std::basic_string<char_t> resolve(std::basic_string_view<char_t> app_name) const;

private:
    Mode m_mode;
};

template <CharT char_t>
Resolver<char_t> create_app_name_resolver();

template <CharT char_t>
Resolver<char_t> create_command_line_resolver();

extern template class Resolver<char>;
extern template class Resolver<wchar_t>;

}  // namespace catter::win::payload
