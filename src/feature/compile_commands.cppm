module;

#include "modules/prelude.h"

module catter:feature.compile_commands;

import :session.handler;

namespace catter::feature {

/// The options of `catter compile-commands` that are its own.
struct CompileCommandsOptions {
    DECO_CFG_START(required = false)

    DecoKVStyled(kotatsu::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"-o", "--output", "--output="},
                 meta_var = "<path>",
                 help = "Write the compilation database to this path.")
    <std::string> output;

    DecoFlag(names = {"--replace"},
             help = "Ignore existing database entries and replace the output file.")
    replace = false;

    DecoFlag(names = {"--save-on-failure"},
             help = "Save collected entries even when the build exits with a non-zero code.")
    save_on_failure = false;

    DecoFlag(names = {"--abort-on-command-failure"},
             help = "Abort when any captured command exits with a non-zero code.")
    abort_on_command_failure = false;

    DecoFlag(names = {"--abort-on-capture-error"},
             help = "Abort when catter reports a command capture error.")
    abort_on_capture_error = false;

    DecoFlag(names = {"-q", "--quiet"}, help = "Suppress informational output.")
    quiet = false;

    DecoFlag(names = {"-v", "--verbose"},
             help = "Print detailed command analysis and generated entries.")
    verbose = false;

    DECO_CFG_END()
};

/// Writes the compilation database of the build's compiler commands.
std::unique_ptr<Handler> make_compile_commands(CompileCommandsOptions options);

}  // namespace catter::feature
