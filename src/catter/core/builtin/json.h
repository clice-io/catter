#pragma once

#include <string>
#include <kota/codec/dyn/dyn.h>

namespace catter::builtin {

using JSON = kota::codec::dyn::Value;

/// `value` as JSON, laid out as JavaScript's `JSON.stringify(value, null, 2)` lays it out, the
/// form catter has always written its JSON in.
std::string to_json(const JSON& value);

}  // namespace catter::builtin
