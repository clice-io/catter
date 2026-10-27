module;

#include "modules/prelude.h"

module catter;

import :driver.app_runner;
import :driver.cli;
import :driver.option;
import :session.process;

namespace catter::driver {

namespace {

struct Options {
    SessionOptions session;

    DECO_CFG_START(required = false)

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "run",
    .description = "Run the build with handlers alone, which decide on and observe its commands.",
    .examples = {{
        .command = R"(catter run --handler "python handler.py" -- make)",
    }},
};

int run(std::span<std::string> args) {
    auto options = parse<Options>(help, args);
    if(!options.has_value()) {
        return 0;
    }
    if(options->session.handlers.empty()) {
        throw cpptrace::runtime_error(
            "run: name a handler to give the build's commands to (--handler, --connect)");
    }
    return exit_like(run_session(nullptr, options->session));
}

[[maybe_unused]] const bool registered = register_subcommand(Subcommand::Run,
                                                             {
                                                                 .name = help.name,
                                                                 .description = help.description,
                                                                 .run = run,
                                                             });

}  // namespace

}  // namespace catter::driver
