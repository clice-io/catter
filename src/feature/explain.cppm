module;

#include "support/prelude.h"

module catter:feature.explain;

namespace catter::feature {

/// What catter makes of the command `argv`, run in `cwd`, as JSON: the compiler or archiver it
/// is, and its analysis.
std::string explain(std::string_view cwd, std::span<const std::string> argv);

}  // namespace catter::feature
