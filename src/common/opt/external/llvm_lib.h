#pragma once

#include <eventide/option/option.h>

namespace catter::opt::llvm_lib {

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
#include <llvm-options-td/llvm-lib-Options.inc>
#undef OPTION
};

const eventide::option::OptTable& table();

}  // namespace catter::opt::llvm_lib
