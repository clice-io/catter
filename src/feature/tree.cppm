module;

#include "support/prelude.h"

module catter:feature.tree;

import :session.handler;

namespace catter::feature {

/// The options of `catter tree` that are its own.
struct TreeOptions {
    KANNOTATE(&depth,
              cli::names = {"-d", "--depth"},
              cli::meta_var = "<n>",
              description = "Limit render depth.")
    std::optional<std::int64_t> depth;

    KANNOTATE(
        &args,
        cli::names = {"-a", "--args"},
        cli::meta_var = "<n>",
        description = "Show the first n arguments after argv[0]. Use -1 to keep all. [default: -1]")
    std::int64_t args = -1;

    KANNOTATE(&arg_width,
              cli::names = {"-w", "--arg-width"},
              cli::meta_var = "<n>",
              description =
                  "Cut each longer argument in its middle to this many characters. [default: 10]")
    std::int64_t arg_width = 10;
};

/// Renders the commands of the build as the tree of the commands that started them.
std::unique_ptr<Handler> make_tree(TreeOptions options);

}  // namespace catter::feature
