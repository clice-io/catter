#include "opt/external/lld_coff.h"

#include <array>
#include <span>

#include "opt/external/tablegen.h"

namespace catter::opt::lld_coff {

namespace kota_opt = kota::option;

namespace detail {

using namespace catter::opt::external_detail;
namespace llvm = catter::opt::external_detail::llvm;

#define OPTTABLE_STR_TABLE_CODE
#include <llvm-options-td/lld-COFF-Options.inc>
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include <llvm-options-td/lld-COFF-Options.inc>
#undef OPTTABLE_PREFIXES_TABLE_CODE

constexpr std::size_t OptionCount = 0
#define OPTION(...) +1
#include <llvm-options-td/lld-COFF-Options.inc>
#undef OPTION
    ;

constexpr std::span<const std::string_view> prefixes(unsigned offset) {
    switch(offset) {
        case 0: return kota_opt::pfx_none;
        case 1: return kota_opt::pfx_double;
        case 3: return pfx_slash_dash_help;
        case 8: return pfx_slash_dash_help_hidden;
        default: return kota_opt::pfx_none;
    }
}

static_assert(OptionPrefixesTable[0] == 0);
static_assert(OptionPrefixesTable[1] == 1 && OptionPrefixesTable[2] == 3);
static_assert(OptionPrefixesTable[3] == 4 && OptionPrefixesTable[4] == 13 &&
              OptionPrefixesTable[5] == 1 && OptionPrefixesTable[6] == 15 &&
              OptionPrefixesTable[7] == 6);
static_assert(OptionPrefixesTable[8] == 4 && OptionPrefixesTable[9] == 18 &&
              OptionPrefixesTable[10] == 9 && OptionPrefixesTable[11] == 15 &&
              OptionPrefixesTable[12] == 6);

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
#include <llvm-options-td/lld-COFF-Options.inc>
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

}  // namespace catter::opt::lld_coff
