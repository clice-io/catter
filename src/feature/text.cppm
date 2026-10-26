module;

#include "modules/prelude.h"

module catter:feature.text;

namespace catter::feature {

/// `text` as a JSON string, quoted; text that is not UTF-8 as a decoder reads it, each ill-formed
/// sequence as U+FFFD.
std::string quote(std::string_view text);

}  // namespace catter::feature
