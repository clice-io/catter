#pragma once

#include <kota/option/option.h>

namespace catter::opt::lld_elf {

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
#include <llvm-options-td/lld-ELF-Options.inc>
#undef OPTION
};

const kota::option::OptTable& table();

}  // namespace catter::opt::lld_elf
