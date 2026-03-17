#pragma once

#include <eventide/option/opt_table.h>

namespace catter::opt::clang {

enum ID : unsigned {
    ID_INVALID = 0,

#define OPTION(PREFIXES_OFFSET,                                                                    \
               NAME_OFFSET,                                                                        \
               ID,                                                                                 \
               KIND,                                                                               \
               GROUP,                                                                              \
               ALIAS,                                                                              \
               ALIAS_ARGS,                                                                         \
               FLAGS,                                                                              \
               VISIBILITY,                                                                         \
               PARAM,                                                                              \
               HELP,                                                                               \
               HELP_TEXTS,                                                                         \
               META_VAR,                                                                           \
               VALUES)                                                                             \
    ID_##ID,
#include <llvm-options-td/clang-Driver-Options.inc>
#undef OPTION
};

enum DriverClass : unsigned {
    DefaultVis = 1u << 0,
    CLOption = 1u << 1,
    CC1Option = 1u << 2,
    CC1AsOption = 1u << 3,
    FC1Option = 1u << 4,
    DXCOption = 1u << 5,
    FlangOption = 1u << 6,
};

const eventide::option::OptTable& table();

}  // namespace catter::opt::clang
