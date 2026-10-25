#pragma once

#include <string>
#include <string_view>

namespace catter::feature {

/// `text` as catter prints it: a byte that is no part of valid UTF-8 as U+FFFD.
std::string printable(std::string_view text);

/// `text`, `printable`, as a JSON string, quoted.
std::string quote(std::string_view text);

}  // namespace catter::feature
