module;

#include "support/prelude.h"

#include "support/messages.h"

module catter:feature.json;

namespace catter::feature {

using JSON = codec::dyn::Value;

/// How `to_json` lays out what it writes.
struct Layout {
    /// As kotatsu's `json::prettify` does, ending with a line end; else on one line.
    bool pretty = true;
};

/// `value` as JSON, laid out as `layout` says; a string that is not UTF-8 as a decoder reads it,
/// each ill-formed sequence as U+FFFD.
std::string to_json(const JSON& value, Layout layout = {});

/// `text` as a JSON string, quoted; text that is not UTF-8 as a decoder reads it, each ill-formed
/// sequence as U+FFFD.
std::string quote(std::string_view text);

/// `command`'s arguments on one line, as catter shows a command: each as it is when plain, else
/// as a JSON string; its executable when it has none.
std::string command_line(const data::command& command);

/// Writes `text` to `file`, whole or not at all, making the directory it goes in when there is
/// none; throws, naming the file as `what`, when it cannot.
void save_file(std::string_view file, std::string_view text, std::string_view what);

}  // namespace catter::feature
