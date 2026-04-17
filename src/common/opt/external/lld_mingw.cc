#include "opt/external/lld_mingw.h"

#include <array>
#include <span>

#include "opt/external/tablegen.h"

namespace catter::opt::lld_mingw {

namespace eo = kota::option;

namespace detail {

using namespace catter::opt::external_detail;
namespace llvm = catter::opt::external_detail::llvm;

#define OPTTABLE_STR_TABLE_CODE
#include <llvm-options-td/lld-MinGW-Options.inc>
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include <llvm-options-td/lld-MinGW-Options.inc>
#undef OPTTABLE_PREFIXES_TABLE_CODE

constexpr std::size_t OptionCount = 0
#define OPTION(...) +1
#include <llvm-options-td/lld-MinGW-Options.inc>
#undef OPTION
    ;

constexpr std::span<const std::string_view> prefixes(unsigned offset) {
    switch(offset) {
        case 0: return eo::pfx_none;
        case 1: return eo::pfx_dash;
        case 3: return eo::pfx_double;
        case 5: return pfx_double_dash_first;
        default: return eo::pfx_none;
    }
}

static_assert(OptionPrefixesTable[0] == 0);
static_assert(OptionPrefixesTable[1] == 1 && OptionPrefixesTable[2] == 1);
static_assert(OptionPrefixesTable[3] == 1 && OptionPrefixesTable[4] == 3);
static_assert(OptionPrefixesTable[5] == 2 && OptionPrefixesTable[6] == 3 &&
              OptionPrefixesTable[7] == 1);

constexpr auto OptionInfos = std::array<eo::OptTable::Info, OptionCount>{
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
    eo::OptTable::Info{                                                                            \
        ._prefixes = prefixes(PREFIXES_OFFSET),                                                    \
        ._prefixed_name = str_at(OptionStrTableStorage, NAME_OFFSET),                              \
        .id = ID_##ID,                                                                             \
        .kind = KIND,                                                                              \
        .group_id = ID_##GROUP,                                                                    \
        .alias_id = ID_##ALIAS,                                                                    \
        .alias_args = ALIAS_ARGS,                                                                  \
        .flags = FLAGS,                                                                            \
        .visibility = VISIBILITY,                                                                  \
        .param = PARAM,                                                                            \
        .help_text = HELP,                                                                         \
        .help_texts_for_variants = DefaultHelpVariants,                                            \
        .meta_var = META_VAR,                                                                      \
    },
#include <llvm-options-td/lld-MinGW-Options.inc>
#undef OPTION
};

}  // namespace detail

const eo::OptTable& table() {
    const static auto opt_table =
        eo::OptTable(std::span<const eo::OptTable::Info>(detail::OptionInfos))
            .set_tablegen_mode(true)
            .set_dash_dash_parsing(true);
    return opt_table;
}

}  // namespace catter::opt::lld_mingw
