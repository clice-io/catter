#include "feature/compile_commands.h"

#include <span>
#include <string>
#include <utility>
#include <kota/async/io/loop.h>
#include <kota/deco/deco.h>

#include "driver/app_runner.h"
#include "driver/cli.h"
#include "driver/driver.h"
#include "driver/option.h"

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
    kota::run(
        async_run(feature::make_compile_commands(std::move(options->database)), options->session));
    return 0;
}

[[maybe_unused]] const bool registered = register_subcommand(Subcommand::CompileCommands,
                                                             {
                                                                 .name = help.name,
                                                                 .description = help.description,
                                                                 .run = run,
                                                             });

}  // namespace

}  // namespace catter::driver
