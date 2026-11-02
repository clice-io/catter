module;

#include "support/prelude.h"

module catter:feature.compile_commands;

import :session.handler;

namespace catter::feature {

/// The options of `catter compile-commands` that are its own.
struct CompileCommandsOptions {
    KANNOTATE(&output,
              cli::names = {"-o", "--output"},
              cli::meta_var = "<path>",
              description = "Write the compilation database to this path.")
    std::string output = "build/compile_commands.json";

    KANNOTATE(&replace,
              description = "Ignore existing database entries and replace the output file.")
    bool replace = false;

    KANNOTATE(
        &save_on_failure,
        description = "Save collected entries even when the build exits with a non-zero code.")
    bool save_on_failure = false;

    KANNOTATE(&abort_on_command_failure,
              description = "Abort when any captured command exits with a non-zero code.")
    bool abort_on_command_failure = false;

    KANNOTATE(&abort_on_capture_error,
              description = "Abort when catter reports a command capture error.")
    bool abort_on_capture_error = false;

    KANNOTATE(&quiet,
              cli::names = {"-q", "--quiet"},
              description = "Suppress informational output.")
    bool quiet = false;

    KANNOTATE(&verbose,
              cli::names = {"-v", "--verbose"},
              description = "Print detailed command analysis and generated entries.")
    bool verbose = false;
};

/// Writes the compilation database of the build's compiler commands.
std::unique_ptr<Handler> make_compile_commands(CompileCommandsOptions options);

}  // namespace catter::feature
