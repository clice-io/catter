#pragma once

#include <string>
#include <string_view>

namespace catter::builtin {

// The built-ins measure, cut and quote text as JavaScript does, in UTF-16 code units, the way
// catter has always printed its trees and JSON.

/// `text` in UTF-16; a byte that is no part of valid UTF-8 becomes U+FFFD.
std::u16string to_utf16(std::string_view text);

/// `text` in UTF-8; a surrogate without its pair keeps its three bytes, as JavaScript writes it.
std::string to_utf8(std::u16string_view text);

/// `text` as catter prints it: a byte that is no part of valid UTF-8 as U+FFFD.
std::string printable(std::string_view text);

/// `text` as a JSON string, quoted, as `JSON.stringify` writes it.
std::string quote(std::u16string_view text);

}  // namespace catter::builtin
