module;

#include "modules/prelude.h"

module catter;

import :driver.cli;
import :driver.option;
import :feature.compile_commands;

namespace catter::driver {

namespace {

struct Options {
    feature::CompileCommandsOptions database;

    SessionOptions session;

    DECO_CFG_START(required = false)

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "compile-commands",
    .description = "Generate a compile_commands.json file from captured compiler commands.",
    .examples =
        {
                   {.command = "catter compile-commands -o build/compile_commands.json -- make"},
                   {
                .command = "catter compile-commands --save-on-failure -- ninja",
                .description =
                    "Merge existing entries and still save partial results from a failed build.",
            }, },
};

int run(std::span<std::string> args) {
    auto options = parse<Options>(help, args);
    if(!options.has_value()) {
        return 0;
    }
    return run_build(feature::make_compile_commands(std::move(options->database)),
                     options->session);
}

[[maybe_unused]] const bool registered = register_subcommand(Subcommand::CompileCommands,
                                                             {
                                                                 .name = help.name,
                                                                 .description = help.description,
                                                                 .run = run,
                                                             });

}  // namespace

}  // namespace catter::driver
