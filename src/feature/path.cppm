module;

#include "modules/prelude.h"

module catter:feature.path;

namespace catter::feature {

/// `file` as the built-ins record a file: from `cwd` when relative, lexically normal. `file` is
/// absolute when it starts at a root of either kind of platform: `/`, `\\` or a drive.
std::string absolute_path(std::string_view cwd, std::string_view file);

}  // namespace catter::feature
