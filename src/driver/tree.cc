module;

#include "modules/prelude.h"

module catter;

import :driver.app_runner;
import :driver.cli;
import :driver.driver;
import :driver.option;
import :feature.tree;

namespace catter::driver {

namespace {

struct Options {
    feature::TreeOptions tree;

    SessionOptions session;

    DECO_CFG_START(required = false)

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "tree",
    .description = "Render captured commands as a command tree.",
};

int run(std::span<std::string> args) {
    auto options = parse<Options>(help, args);
    if(!options.has_value()) {
        return 0;
    }
    check_at_least<Options>(help, "--depth", options->tree.depth.as_optional(), 0);
    check_at_least<Options>(help, "--args", *options->tree.args, -1);
    check_at_least<Options>(help, "--arg-width", *options->tree.arg_width, 4);
    kota::run(async_run(feature::make_tree(std::move(options->tree)), options->session));
    return 0;
}

[[maybe_unused]] const bool registered = register_subcommand(Subcommand::Tree,
                                                             {
                                                                 .name = help.name,
                                                                 .description = help.description,
                                                                 .run = run,
                                                             });

}  // namespace

}  // namespace catter::driver
