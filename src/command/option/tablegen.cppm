module;

#include "modules/prelude.h"

module catter.command:option.tablegen;

namespace catter::option::external_detail {

namespace option = kotatsu::option;

namespace llvm {

struct StringTable {
    using Offset = unsigned;

    const char* storage;

    constexpr StringTable(const char* storage) : storage(storage) {}
};

}  // namespace llvm

constexpr std::string_view str_at(const char* storage, unsigned offset) {
    const char* begin = storage + offset;
    std::size_t size = 0;
    while(begin[size] != '\0') {
        ++size;
    }
    return {begin, size};
}

constexpr inline std::string_view _pfx_double_dash_first_storage[] = {"--", "-"};
constexpr inline auto pfx_double_dash_first =
    std::span<const std::string_view>(_pfx_double_dash_first_storage);

constexpr inline std::string_view _pfx_slash_dash_help_storage[] = {"/", "-", "/?", "-?"};
constexpr inline auto pfx_slash_dash_help =
    std::span<const std::string_view>(_pfx_slash_dash_help_storage);

constexpr inline std::string_view _pfx_slash_dash_help_hidden_storage[] = {"/??",
                                                                           "-??",
                                                                           "/?",
                                                                           "-?"};
constexpr inline auto pfx_slash_dash_help_hidden =
    std::span<const std::string_view>(_pfx_slash_dash_help_hidden_storage);

constexpr inline auto Group = option::Kind::Group;
constexpr inline auto Input = option::Kind::Input;
constexpr inline auto Unknown = option::Kind::Unknown;
constexpr inline auto Flag = option::Kind::Flag;
constexpr inline auto Joined = option::Kind::Joined;
constexpr inline auto Values = option::Kind::Values;
constexpr inline auto Separate = option::Kind::Separate;
constexpr inline auto RemainingArgs = option::Kind::RemainingArgs;
constexpr inline auto RemainingArgsJoined = option::Kind::RemainingArgsJoined;
constexpr inline auto CommaJoined = option::Kind::CommaJoined;
constexpr inline auto MultiArg = option::Kind::MultiArg;
constexpr inline auto JoinedOrSeparate = option::Kind::JoinedOrSeparate;
constexpr inline auto JoinedAndSeparate = option::Kind::JoinedAndSeparate;

constexpr inline unsigned HelpHidden = option::HelpHidden;
constexpr inline unsigned RenderAsInput = option::RenderAsInput;
constexpr inline unsigned RenderJoined = option::RenderJoined;
constexpr inline unsigned Ignored = 1u << 4;
constexpr inline unsigned LinkOption = 1u << 5;
constexpr inline unsigned LinkerInput = 1u << 6;
constexpr inline unsigned NoArgumentUnused = 1u << 7;
constexpr inline unsigned NoXarchOption = 1u << 8;
constexpr inline unsigned TargetSpecific = 1u << 9;
constexpr inline unsigned Unsupported = 1u << 10;

constexpr inline unsigned DefaultVis = option::DefaultVis;
constexpr inline unsigned CLOption = 1u << 1;
constexpr inline unsigned CC1Option = 1u << 2;
constexpr inline unsigned CC1AsOption = 1u << 3;
constexpr inline unsigned FC1Option = 1u << 4;
constexpr inline unsigned DXCOption = 1u << 5;
constexpr inline unsigned FlangOption = 1u << 6;

}  // namespace catter::option::external_detail
