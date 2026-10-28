module;

#include "modules/prelude.h"

module catter:feature.profile;

import :session.handler;

namespace catter::feature {

/// The options of `catter profile` that are its own.
struct ProfileOptions {
    DECO_CFG_START(required = false)

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"-o", "--output", "--output="},
                 meta_var = "<path>",
                 help = "Write the trace to this path. [default: trace.json]")
    <std::string> output = std::string("trace.json");

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"--top", "--top="},
                 meta_var = "<n>",
                 help = "List the n commands that took the longest. [default: 5]")
    <std::int64_t> top = 5;

    DecoFlag(names = {"-q", "--quiet"},
             help = "Print no summary, nor why catter could not capture a command.")
    quiet = false;

    DECO_CFG_END()
};

/// Writes the timeline of the build's commands as a trace that Perfetto and chrome://tracing
/// open, and prints what it shows.
std::unique_ptr<Handler> make_profile(ProfileOptions options);

}  // namespace catter::feature
