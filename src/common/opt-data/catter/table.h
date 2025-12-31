#pragma once

#include "option/opt_table.h"

namespace catter::optdata::main {
enum OptionID {
    OPT_INVALID = 0,
    OPT_INPUT = 1,
    OPT_UNKNOWN = 2,
    OPT_HELP,
    OPT_HELP_SHORT,
    OPT_SCRIPT
};

extern opt::OptTable catter_proxy_opt_table;
}  // namespace catter::optdata::main
