#include "option/clang.h"

#include <array>
#include <span>
#include <string_view>
#include <kota/deco/option.h>

namespace catter::opt::clang {

namespace kota_opt = kota::option;

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
// `kota::option::Option` without pulling in LLVM's option library.
enum Flag : unsigned {
    HelpHidden = kota_opt::HelpHidden,
    RenderAsInput = kota_opt::RenderAsInput,
    RenderJoined = kota_opt::RenderJoined,
    Ignored = 1u << 4,
    LinkOption = 1u << 5,
    LinkerInput = 1u << 6,
    NoArgumentUnused = 1u << 7,
    NoXarchOption = 1u << 8,
    TargetSpecific = 1u << 9,
    Unsupported = 1u << 10,
};

// The generated prefix table only uses a small fixed set of layouts for clang
// driver options, so we map the encoded table offsets to the corresponding
// `kota::option` prefix spans directly.
constexpr std::span<const std::string_view> prefixes(unsigned offset) {
    switch(offset) {
        case 0: return kota_opt::pfx_none;
        case 1: return kota_opt::pfx_dash;
        case 3: return kota_opt::pfx_dash_double;
        case 6: return kota_opt::pfx_double;
        case 8: return kota_opt::pfx_all;
        case 12: return kota_opt::pfx_slash_dash;
        default: return kota_opt::pfx_none;
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

#define Group kota_opt::Kind::Group
#define Input kota_opt::Kind::Input
#define Unknown kota_opt::Kind::Unknown
#define Flag kota_opt::Kind::Flag
#define Joined kota_opt::Kind::Joined
#define Values kota_opt::Kind::Values
#define Separate kota_opt::Kind::Separate
#define RemainingArgs kota_opt::Kind::RemainingArgs
#define RemainingArgsJoined kota_opt::Kind::RemainingArgsJoined
#define CommaJoined kota_opt::Kind::CommaJoined
#define MultiArg kota_opt::Kind::MultiArg
#define JoinedOrSeparate kota_opt::Kind::JoinedOrSeparate
#define JoinedAndSeparate kota_opt::Kind::JoinedAndSeparate

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
        .prefixed_name = str_at(NAME_OFFSET),                                                      \
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

const kota_opt::OptTable& table() {
    const static auto opt_table = [] {
        auto table =
            kota_opt::OptTable(std::span<const kota_opt::Option>(detail::OptionInfos), false, {});
        table.tablegen_mode = true;
        return table;
    }();
    return opt_table;
}

}  // namespace catter::opt::clang
