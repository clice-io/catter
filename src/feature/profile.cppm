module;

#include "support/prelude.h"

module catter:feature.profile;

import :session.handler;

namespace catter::feature {

/// The options of `catter profile` that are its own.
struct ProfileOptions {
    KANNOTATE(&output,
              cli::names = {"-o", "--output"},
              cli::meta_var = "<path>",
              description = "Write the trace to this path. [default: trace.json]")
    std::string output = "trace.json";

    KANNOTATE(&top,
              cli::meta_var = "<n>",
              description = "List the n commands that took the longest. [default: 5]")
    std::int64_t top = 5;

    KANNOTATE(&quiet,
              cli::names = {"-q", "--quiet"},
              description = "Print no summary, nor why catter could not capture a command.")
    bool quiet = false;
};

/// Writes the timeline of the build's commands as a trace that Perfetto and chrome://tracing
/// open, and prints what it shows.
std::unique_ptr<Handler> make_profile(ProfileOptions options);

}  // namespace catter::feature
