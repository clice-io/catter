module;

#include "modules/prelude.h"

module catter:feature.json;

namespace catter::feature {

using JSON = kota::codec::dyn::Value;

/// `value` as JSON, laid out by kotatsu's `json::prettify` and ending with a line end; a string
/// that is not UTF-8 as a decoder reads it, each ill-formed sequence as U+FFFD.
std::string to_json(const JSON& value);

/// `text` as a JSON string, quoted; text that is not UTF-8 as a decoder reads it, each ill-formed
/// sequence as U+FFFD.
std::string quote(std::string_view text);

}  // namespace catter::feature
