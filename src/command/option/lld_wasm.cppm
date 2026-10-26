module;

#include "modules/prelude.h"

export module catter.command:option.lld_wasm;

export namespace catter::option::lld_wasm {

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
#include "llvm-options-td/lld-wasm-Options.inc"
#undef OPTION
};

const kota::option::OptTable& table();

}  // namespace catter::option::lld_wasm
