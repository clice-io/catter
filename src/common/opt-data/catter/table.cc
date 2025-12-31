#include "opt-data/catter/table.h"
#include "option/util.h"
#include <array>
#include <option/opt_table.h>
#include <option/option.h>

namespace {
using namespace catter;
// clang-format off
constexpr auto opt_infos = std::array{
        opt::OptTable::Info::input(
            optdata::main::OPT_INPUT
        ),
        opt::OptTable::Info::unknown(
            optdata::main::OPT_UNKNOWN
        ),
        opt::OptTable::Info::unaliased_one(
            catter::opt::pfx_double,
            "--help",
            optdata::main::OPT_HELP,
            opt::Option::FlagClass,
            0,
            "Display available options.",
            ""
        ),
        opt::OptTable::Info::unaliased_one(
            catter::opt::pfx_dash,
            "-h",
            optdata::main::OPT_HELP_SHORT,
            opt::Option::FlagClass,
            0,
            "Display available options.",
            ""
        ).alias_of(optdata::main::OPT_HELP),

        opt::OptTable::Info::unaliased_one(
            catter::opt::pfx_dash,
            "-s",
            optdata::main::OPT_SCRIPT,
            opt::Option::SeparateClass,
            1,
            "Path to the script to execute or script::<inner script> to execute inner script.",
            "<executable.js>"
        ),
    };
// clang-format on
}  // namespace

namespace catter::optdata::main {
opt::OptTable catter_proxy_opt_table =
    opt::OptTable(std::span<const opt::OptTable::Info>(opt_infos))
        .set_tablegen_mode(false)
        .set_dash_dash_parsing(true)
        .set_dash_dash_as_single_pack(true);
}  // namespace catter::optdata::main
