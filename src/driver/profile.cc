module;

#include "modules/prelude.h"

module catter;

import :driver.cli;
import :driver.option;
import :feature.profile;

namespace catter::driver {

namespace {

struct Options {
    feature::ProfileOptions profile;

    SessionOptions session;

    DECO_CFG_START(required = false)

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "profile",
    .description = "Write the timeline of the build's commands as a trace for Perfetto.",
    .examples =
        {
                   {.command = "catter profile -- make -j8"},
                   {
                .command = "catter profile -o build.json --top 10 -- ninja",
                .description =
                    "Write the trace to build.json, and list the 10 commands that took the longest.",
            }, },
};

int run(std::span<std::string> args) {
    auto options = parse<Options>(help, args);
    if(!options.has_value()) {
        return 0;
    }
    check_at_least<Options>(help, "--top", *options->profile.top, 0);
    return run_build(feature::make_profile(std::move(options->profile)), options->session);
}

[[maybe_unused]] const bool registered = register_subcommand(Subcommand::Profile,
                                                             {
                                                                 .name = help.name,
                                                                 .description = help.description,
                                                                 .run = run,
                                                             });

}  // namespace

}  // namespace catter::driver
