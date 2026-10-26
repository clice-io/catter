module;

#include "modules/prelude.h"

module catter:feature.tree;

import :session.handler;

namespace catter::feature {

/// The options of `catter tree` that are its own.
struct TreeOptions {
    DECO_CFG_START(required = false)

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"-d", "--depth", "--depth="},
                 meta_var = "<n>",
                 help = "Limit render depth.")
    <std::int64_t> depth;

    DecoKVStyled(
        kota::deco::decl::KVStyle::JoinedOrSeparate,
        names = {"-a", "--args", "--args="},
        meta_var = "<n>",
        help = "Show the first n arguments after argv[0]. Use -1 to keep all. [default: -1]")
    <std::int64_t> args = -1;

    DecoKVStyled(
        kota::deco::decl::KVStyle::JoinedOrSeparate,
        names = {"-w", "--arg-width", "--arg-width="},
        meta_var = "<n>",
        help = "Cut each longer argument in its middle to this many characters. [default: 10]")
    <std::int64_t> arg_width = 10;

    DECO_CFG_END()
};

/// Renders the commands of the build as the tree of the commands that started them.
std::unique_ptr<Handler> make_tree(TreeOptions options);

}  // namespace catter::feature
