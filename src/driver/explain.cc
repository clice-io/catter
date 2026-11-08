module;

#include "support/prelude.h"

module catter;

import :driver.cli;
import :driver.option;
import :feature.explain;

namespace catter::driver {

namespace {

struct Options {
    DirectoryOption directory;

    KANNOTATE(&command,
              cli::rest = true,
              cli::meta_var = "<command>",
              description = "The command to analyze.")
    std::vector<std::string> command;
};

const Help help{
    .name = "explain",
    .description = "Print how catter analyzes the command after --, without running it.",
    .examples = {{
        .command = "catter explain -- clang++ -c src/main.cc -o main.o",
        .description = "The compiler, its mode, target, sources and outputs.",
    }},
};

int run(std::span<std::string> args) {
    auto options = parse<Options>(help, args);
    if(!options.has_value()) {
        return 0;
    }
    if(options->command.empty()) {
        throw cpptrace::runtime_error("explain: give the command to analyze after --");
    }
    std::print("{}", feature::explain(options->directory.directory.path, options->command));
    return 0;
}

[[maybe_unused]] const bool registered = register_subcommand(Subcommand::Explain,
                                                             {
                                                                 .name = help.name,
                                                                 .description = help.description,
                                                                 .run = run,
                                                             });

}  // namespace

}  // namespace catter::driver
