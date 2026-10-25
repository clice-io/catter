#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <kota/meta/enum.h>

namespace catter::driver {

/// The subcommands, in the order `catter --help` lists them.
enum class Subcommand : std::uint8_t {
    CompileCommands,
    Tree,
    Targets,
    Explain,
    Run,
};

/// A subcommand as its file registers it: its name and line of `catter --help`, and what runs
/// it on the arguments after its name, returning the exit code.
struct SubcommandEntry {
    std::string_view name;

    std::string_view description;

    int (*run)(std::span<std::string> args) = nullptr;
};

inline auto& subcommand_entries() {
    static std::array<SubcommandEntry, kota::meta::reflection<Subcommand>::member_count> entries;
    return entries;
}

/// Registers a subcommand, from the initializer of a variable at namespace scope of its file.
inline bool register_subcommand(Subcommand subcommand, SubcommandEntry entry) {
    auto& registered = subcommand_entries()[std::to_underlying(subcommand)];
    assert(registered.run == nullptr && "a subcommand registers once");
    registered = entry;
    return true;
}

}  // namespace catter::driver
