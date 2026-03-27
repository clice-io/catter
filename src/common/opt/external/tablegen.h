#pragma once

#include <array>
#include <span>
#include <string_view>

#include <eventide/option/option.h>

namespace catter::opt::external_detail {

namespace eo = eventide::option;

namespace llvm {

struct StringTable {
    using Offset = unsigned;

    const char* storage;

    constexpr StringTable(const char* storage) : storage(storage) {}
};

}  // namespace llvm

constexpr auto DefaultHelpVariants = std::array<std::pair<std::array<unsigned, 2>, const char*>, 1>{
    std::pair{std::array<unsigned, 2>{0, 0}, nullptr},
};

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

constexpr inline auto Group = eo::Option::GroupClass;
constexpr inline auto Input = eo::Option::InputClass;
constexpr inline auto Unknown = eo::Option::UnknownClass;
constexpr inline auto Flag = eo::Option::FlagClass;
constexpr inline auto Joined = eo::Option::JoinedClass;
constexpr inline auto Values = eo::Option::ValuesClass;
constexpr inline auto Separate = eo::Option::SeparateClass;
constexpr inline auto RemainingArgs = eo::Option::RemainingArgsClass;
constexpr inline auto RemainingArgsJoined = eo::Option::RemainingArgsJoinedClass;
constexpr inline auto CommaJoined = eo::Option::CommaJoinedClass;
constexpr inline auto MultiArg = eo::Option::MultiArgClass;
constexpr inline auto JoinedOrSeparate = eo::Option::JoinedOrSeparateClass;
constexpr inline auto JoinedAndSeparate = eo::Option::JoinedAndSeparateClass;

constexpr inline unsigned HelpHidden = eo::HelpHidden;
constexpr inline unsigned RenderAsInput = eo::RenderAsInput;
constexpr inline unsigned RenderJoined = eo::RenderJoined;
constexpr inline unsigned Ignored = 1u << 4;
constexpr inline unsigned LinkOption = 1u << 5;
constexpr inline unsigned LinkerInput = 1u << 6;
constexpr inline unsigned NoArgumentUnused = 1u << 7;
constexpr inline unsigned NoXarchOption = 1u << 8;
constexpr inline unsigned TargetSpecific = 1u << 9;
constexpr inline unsigned Unsupported = 1u << 10;

constexpr inline unsigned DefaultVis = eo::DefaultVis;
constexpr inline unsigned CLOption = 1u << 1;
constexpr inline unsigned CC1Option = 1u << 2;
constexpr inline unsigned CC1AsOption = 1u << 3;
constexpr inline unsigned FC1Option = 1u << 4;
constexpr inline unsigned DXCOption = 1u << 5;
constexpr inline unsigned FlangOption = 1u << 6;

}  // namespace catter::opt::external_detail
