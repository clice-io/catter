#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "winapi.h"

namespace catter::win::payload {

template <CharT char_t>
class Resolver {
public:
    explicit Resolver(std::vector<std::basic_string<char_t>> search_paths) :
        m_search_paths(std::move(search_paths)) {}

    std::basic_string<char_t> resolve(std::basic_string_view<char_t> app_name) const;

private:
    static std::basic_string<char_t>
        join_search_paths(const std::vector<std::basic_string<char_t>>& search_paths);
    std::vector<std::basic_string<char_t>> m_search_paths;
};

template <CharT char_t>
Resolver<char_t> create_app_name_resolver();

template <CharT char_t>
Resolver<char_t> create_command_line_resolver();

extern template class Resolver<char>;
extern template class Resolver<wchar_t>;

}  // namespace catter::win::payload
