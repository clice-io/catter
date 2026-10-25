#include "feature/explain.h"

#include <print>
#include <span>
#include <string>
#include <vector>
#include <cpptrace/exceptions.hpp>
#include <kota/deco/deco.h>

#include "driver/cli.h"
#include "driver/driver.h"
#include "driver/option.h"
#include "support/encoding.h"

namespace catter::driver {

namespace {

struct Options {
    DirectoryOption directory;

    DECO_CFG_START(required = false)

    DecoPack(meta_var = "<command>", help = "The command to analyze.")
    <std::vector<std::string>> command = std::vector<std::string>{};

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
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
    if(options->command->empty()) {
        throw cpptrace::runtime_error("explain: give the command to analyze after --");
    }
    std::print("{}",
               feature::explain(to_utf8(options->directory.directory->path), *options->command));
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
