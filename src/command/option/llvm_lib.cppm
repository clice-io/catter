module;

#include "modules/prelude.h"

export module catter.command:option.llvm_lib;

export namespace catter::option::llvm_lib {

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
               VALUES,                                                                             \
               SUBCOMMANDIDS_OFFSET)                                                               \
    ID_##ID,
#include "llvm-options-td/llvm-lib-Options.inc"
#undef OPTION
};

const kotatsu::option::OptTable& table();

}  // namespace catter::option::llvm_lib
