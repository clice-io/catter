#include "opt/external/clang.h"

#include <array>
#include <span>
#include <string_view>

#include <eventide/option/option.h>

namespace catter::opt::clang {

namespace eo = eventide::option;

namespace detail {

namespace llvm {

struct StringTable {
    using Offset = unsigned;

    const char* storage;

    constexpr StringTable(const char* storage) : storage(storage) {}
};

}  // namespace llvm

#define OPTTABLE_STR_TABLE_CODE
#include <llvm-options-td/clang-Driver-Options.inc>
#undef OPTTABLE_STR_TABLE_CODE

#define OPTTABLE_PREFIXES_TABLE_CODE
#include <llvm-options-td/clang-Driver-Options.inc>
#undef OPTTABLE_PREFIXES_TABLE_CODE

constexpr std::size_t OptionCount = 0
#define OPTION(...) +1
#include <llvm-options-td/clang-Driver-Options.inc>
#undef OPTION
    ;

// `clang-Driver-Options.inc` reuses LLVM driver flag names directly in the generated
// `OPTION(...)` rows. We mirror the bits here so the table can be embedded into
// `eventide::option::OptTable::Info` without pulling in LLVM's option library.
enum Flag : unsigned {
    HelpHidden = eo::HelpHidden,
    RenderAsInput = eo::RenderAsInput,
    RenderJoined = eo::RenderJoined,
    Ignored = 1u << 4,
    LinkOption = 1u << 5,
    LinkerInput = 1u << 6,
    NoArgumentUnused = 1u << 7,
    NoXarchOption = 1u << 8,
    TargetSpecific = 1u << 9,
    Unsupported = 1u << 10,
};

constexpr auto DefaultHelpVariants = std::array<std::pair<std::array<unsigned, 2>, const char*>, 1>{
    std::pair{std::array<unsigned, 2>{0, 0}, nullptr},
};

// The generated prefix table only uses a small fixed set of layouts for clang
// driver options, so we map the encoded table offsets to the corresponding
// `eventide::option` prefix spans directly.
constexpr std::span<const std::string_view> prefixes(unsigned offset) {
    switch(offset) {
        case 0: return eo::pfx_none;
        case 1: return eo::pfx_dash;
        case 3: return eo::pfx_dash_double;
        case 6: return eo::pfx_double;
        case 8: return eo::pfx_all;
        case 12: return eo::pfx_slash_dash;
        default: return eo::pfx_none;
    }
}

constexpr std::string_view str_at(unsigned offset) {
    const char* begin = OptionStrTableStorage + offset;
    std::size_t size = 0;
    while(begin[size] != '\0') {
        ++size;
    }
    return {begin, size};
}

// Guard the hard-coded offset mapping above. If LLVM changes the generated
// prefix table layout, fail at compile time instead of silently mis-parsing
// option prefixes.
static_assert(OptionPrefixesTable[0] == 0);
static_assert(OptionPrefixesTable[1] == 1 && OptionPrefixesTable[2] == 1);
static_assert(OptionPrefixesTable[3] == 2 && OptionPrefixesTable[4] == 1 &&
              OptionPrefixesTable[5] == 3);
static_assert(OptionPrefixesTable[6] == 1 && OptionPrefixesTable[7] == 3);
static_assert(OptionPrefixesTable[8] == 3 && OptionPrefixesTable[9] == 3 &&
              OptionPrefixesTable[10] == 6 && OptionPrefixesTable[11] == 1);
static_assert(OptionPrefixesTable[12] == 2 && OptionPrefixesTable[13] == 6 &&
              OptionPrefixesTable[14] == 1);

#define Group eo::Option::GroupClass
#define Input eo::Option::InputClass
#define Unknown eo::Option::UnknownClass
#define Flag eo::Option::FlagClass
#define Joined eo::Option::JoinedClass
#define Values eo::Option::ValuesClass
#define Separate eo::Option::SeparateClass
#define RemainingArgs eo::Option::RemainingArgsClass
#define RemainingArgsJoined eo::Option::RemainingArgsJoinedClass
#define CommaJoined eo::Option::CommaJoinedClass
#define MultiArg eo::Option::MultiArgClass
#define JoinedOrSeparate eo::Option::JoinedOrSeparateClass
#define JoinedAndSeparate eo::Option::JoinedAndSeparateClass

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
        ._prefixed_name = str_at(NAME_OFFSET),                                                     \
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
#include <llvm-options-td/clang-Driver-Options.inc>
#undef OPTION
};

#undef Group
#undef Input
#undef Unknown
#undef Flag
#undef Joined
#undef Values
#undef Separate
#undef RemainingArgs
#undef RemainingArgsJoined
#undef CommaJoined
#undef MultiArg
#undef JoinedOrSeparate
#undef JoinedAndSeparate

}  // namespace detail

const eo::OptTable& table() {
    const static auto opt_table =
        eo::OptTable(std::span<const eo::OptTable::Info>(detail::OptionInfos))
            .set_tablegen_mode(true)
            .set_dash_dash_parsing(true);
    return opt_table;
}

}  // namespace catter::opt::clang
