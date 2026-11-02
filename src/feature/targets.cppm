module;

#include "support/prelude.h"

module catter:feature.targets;

import :session.handler;

namespace catter::feature {

/// The options of `catter targets` that are its own.
struct TargetsOptions {
    KANNOTATE(&depth,
              cli::names = {"-d", "--depth"},
              cli::meta_var = "<n>",
              description = "Limit render depth.")
    std::optional<std::int64_t> depth;
};

/// Renders the files the build makes as the forest of the files each is made from.
std::unique_ptr<Handler> make_targets(TargetsOptions options);

}  // namespace catter::feature
