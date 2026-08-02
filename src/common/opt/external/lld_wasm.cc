#include "opt/external/lld_wasm.h"

#include <array>
#include <span>

#include "opt/external/tablegen.h"

namespace catter::opt::lld_wasm {

namespace kota_opt = kota::option;

namespace detail {

using namespace catter::opt::external_detail;
namespace llvm = catter::opt::external_detail::llvm;

#define OPTTABLE_STR_TABLE_CODE
#include <llvm-options-td/lld-wasm-Options.inc>
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include <llvm-options-td/lld-wasm-Options.inc>
#undef OPTTABLE_PREFIXES_TABLE_CODE

constexpr std::size_t OptionCount = 0
#define OPTION(...) +1
#include <llvm-options-td/lld-wasm-Options.inc>
#undef OPTION
    ;

constexpr std::span<const std::string_view> prefixes(unsigned offset) {
    switch(offset) {
        case 0: return kota_opt::pfx_none;
        case 1: return kota_opt::pfx_dash;
        case 3: return kota_opt::pfx_double;
        case 5: return pfx_double_dash_first;
        default: return kota_opt::pfx_none;
    }
}

static_assert(OptionPrefixesTable[0] == 0);
static_assert(OptionPrefixesTable[1] == 1 && OptionPrefixesTable[2] == 1);
static_assert(OptionPrefixesTable[3] == 1 && OptionPrefixesTable[4] == 3);
static_assert(OptionPrefixesTable[5] == 2 && OptionPrefixesTable[6] == 3 &&
              OptionPrefixesTable[7] == 1);

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
#include <llvm-options-td/lld-wasm-Options.inc>
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

}  // namespace catter::opt::lld_wasm
