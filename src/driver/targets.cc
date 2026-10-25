#include "feature/targets.h"

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
    feature::TargetsOptions targets;

    SessionOptions session;

    DECO_CFG_START(required = false)

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "targets",
    .description = "Render captured build targets as a flat target forest.",
};

int run(std::span<std::string> args) {
    auto options = parse<Options>(help, args);
    if(!options.has_value()) {
        return 0;
    }
    check_at_least<Options>(help, "--depth", options->targets.depth.as_optional(), 0);
    kota::run(async_run(feature::make_targets(std::move(options->targets)), options->session));
    return 0;
}

[[maybe_unused]] const bool registered = register_command(Command::Targets,
                                                          {
                                                              .name = help.name,
                                                              .description = help.description,
                                                              .run = run,
                                                          });

}  // namespace

}  // namespace catter::driver
