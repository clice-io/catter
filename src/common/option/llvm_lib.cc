#include "option/llvm_lib.h"

#include <array>
#include <span>

#include "option/tablegen.h"

namespace catter::opt::llvm_lib {

namespace kota_opt = kota::option;

namespace detail {

using namespace catter::opt::external_detail;
namespace llvm = catter::opt::external_detail::llvm;

#define OPTTABLE_STR_TABLE_CODE
#include <llvm-options-td/llvm-lib-Options.inc>
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include <llvm-options-td/llvm-lib-Options.inc>
#undef OPTTABLE_PREFIXES_TABLE_CODE

constexpr std::size_t OptionCount = 0
#define OPTION(...) +1
#include <llvm-options-td/llvm-lib-Options.inc>
#undef OPTION
    ;

constexpr std::span<const std::string_view> prefixes(unsigned offset) {
    switch(offset) {
        case 0: return kota_opt::pfx_none;
        case 1: return pfx_slash_dash_help;
        case 6: return pfx_slash_dash_help_hidden;
        default: return kota_opt::pfx_none;
    }
}

static_assert(OptionPrefixesTable[0] == 0);
static_assert(OptionPrefixesTable[1] == 4 && OptionPrefixesTable[2] == 10 &&
              OptionPrefixesTable[3] == 1 && OptionPrefixesTable[4] == 12 &&
              OptionPrefixesTable[5] == 3);
static_assert(OptionPrefixesTable[6] == 4 && OptionPrefixesTable[7] == 15 &&
              OptionPrefixesTable[8] == 6 && OptionPrefixesTable[9] == 12 &&
              OptionPrefixesTable[10] == 3);

constexpr auto OptionInfos = std::array<kota_opt::Option, OptionCount>{
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
    kota_opt::Option{                                                                              \
        .prefixes = prefixes(PREFIXES_OFFSET),                                                     \
        .prefixed_name = str_at(OptionStrTableStorage, NAME_OFFSET),                               \
        .id = ID_##ID,                                                                             \
        .kind = KIND,                                                                              \
        .group_id = ID_##GROUP,                                                                    \
        .alias_id = ID_##ALIAS,                                                                    \
        .alias_args = ALIAS_ARGS,                                                                  \
        .flags = FLAGS,                                                                            \
        .visibility = VISIBILITY,                                                                  \
        .num_args = PARAM,                                                                         \
        .help_text = HELP,                                                                         \
        .meta_var = META_VAR,                                                                      \
    },
#include <llvm-options-td/llvm-lib-Options.inc>
#undef OPTION
};

}  // namespace detail

const kota_opt::OptTable& table() {
    const static auto opt_table = [] {
        auto table =
            kota_opt::OptTable(std::span<const kota_opt::Option>(detail::OptionInfos), false, {});
        table.tablegen_mode = true;
        return table;
    }();
    return opt_table;
}

}  // namespace catter::opt::llvm_lib
