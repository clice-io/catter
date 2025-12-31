#include "opt-data/catter-proxy/table.h"
#include "option/util.h"
#include <array>
#include <option/opt_table.h>
#include <option/option.h>

namespace {
using namespace catter;
// clang-format off
constexpr auto opt_infos = std::array{
        opt::OptTable::Info::input(
            optdata::catter_proxy::OPT_INPUT
        ),
        opt::OptTable::Info::unknown(
            optdata::catter_proxy::OPT_UNKNOWN
        ),
        opt::OptTable::Info::unaliased_one(
            catter::opt::pfx_dash,
            "-p",
            optdata::catter_proxy::OPT_PARENT_ID,
            opt::Option::SeparateClass,
            1,
            "Specify the parent process ID.",
            "<parent id>"
        ),
        opt::OptTable::Info::unaliased_one(
            catter::opt::pfx_dash_double,
            "--exec",
            optdata::catter_proxy::OPT_EXEC,
            opt::Option::SeparateClass,
            1,
            "Specify the executable to run.",
            "<executable>"
        ),
    };
// clang-format on
}  // namespace

namespace catter::optdata::catter_proxy {
opt::OptTable catter_proxy_opt_table =
    opt::OptTable(std::span<const opt::OptTable::Info>(opt_infos))
        .set_tablegen_mode(false)
        .set_dash_dash_parsing(true);
}  // namespace catter::optdata::catter_proxy
