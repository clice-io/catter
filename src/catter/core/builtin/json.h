#pragma once

#include <string>
#include <kota/codec/dyn/dyn.h>

namespace catter::builtin {

using JSON = kota::codec::dyn::Value;

/// `value` as JSON, laid out by kotatsu's `json::prettify` and ending with a line end; a byte of a
/// string that is no part of valid UTF-8 as U+FFFD.
std::string to_json(const JSON& value);

}  // namespace catter::builtin
