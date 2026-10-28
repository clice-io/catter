module;

#include "modules/prelude.h"

module catter:feature.json;

namespace catter::feature {

using JSON = kota::codec::dyn::Value;

/// How `to_json` lays out what it writes.
enum class Layout : std::uint8_t {
    /// As kotatsu's `json::prettify` does, ending with a line end.
    Pretty,
    /// On one line, without a line end.
    Line,
};

/// `value` as JSON, laid out as `layout` says; a string that is not UTF-8 as a decoder reads it,
/// each ill-formed sequence as U+FFFD.
std::string to_json(const JSON& value, Layout layout = Layout::Pretty);

/// `text` as a JSON string, quoted; text that is not UTF-8 as a decoder reads it, each ill-formed
/// sequence as U+FFFD.
std::string quote(std::string_view text);

}  // namespace catter::feature
