#pragma once

#include <array>
#include <span>
#include <string_view>
#include <kota/deco/option.h>

namespace catter::opt::external_detail {

namespace kota_opt = kota::option;

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

constexpr inline auto Group = kota_opt::Kind::Group;
constexpr inline auto Input = kota_opt::Kind::Input;
constexpr inline auto Unknown = kota_opt::Kind::Unknown;
constexpr inline auto Flag = kota_opt::Kind::Flag;
constexpr inline auto Joined = kota_opt::Kind::Joined;
constexpr inline auto Values = kota_opt::Kind::Values;
constexpr inline auto Separate = kota_opt::Kind::Separate;
constexpr inline auto RemainingArgs = kota_opt::Kind::RemainingArgs;
constexpr inline auto RemainingArgsJoined = kota_opt::Kind::RemainingArgsJoined;
constexpr inline auto CommaJoined = kota_opt::Kind::CommaJoined;
constexpr inline auto MultiArg = kota_opt::Kind::MultiArg;
constexpr inline auto JoinedOrSeparate = kota_opt::Kind::JoinedOrSeparate;
constexpr inline auto JoinedAndSeparate = kota_opt::Kind::JoinedAndSeparate;

constexpr inline unsigned HelpHidden = kota_opt::HelpHidden;
constexpr inline unsigned RenderAsInput = kota_opt::RenderAsInput;
constexpr inline unsigned RenderJoined = kota_opt::RenderJoined;
constexpr inline unsigned Ignored = 1u << 4;
constexpr inline unsigned LinkOption = 1u << 5;
constexpr inline unsigned LinkerInput = 1u << 6;
constexpr inline unsigned NoArgumentUnused = 1u << 7;
constexpr inline unsigned NoXarchOption = 1u << 8;
constexpr inline unsigned TargetSpecific = 1u << 9;
constexpr inline unsigned Unsupported = 1u << 10;

constexpr inline unsigned DefaultVis = kota_opt::DefaultVis;
constexpr inline unsigned CLOption = 1u << 1;
constexpr inline unsigned CC1Option = 1u << 2;
constexpr inline unsigned CC1AsOption = 1u << 3;
constexpr inline unsigned FC1Option = 1u << 4;
constexpr inline unsigned DXCOption = 1u << 5;
constexpr inline unsigned FlangOption = 1u << 6;

}  // namespace catter::opt::external_detail
