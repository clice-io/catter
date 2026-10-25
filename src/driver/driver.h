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
enum class Command : std::uint8_t {
    CompileCommands,
    Tree,
    Targets,
    Explain,
    Run,
};

/// A subcommand as its file registers it: its name and line of `catter --help`, and what runs
/// it on the arguments after its name, returning the exit code.
struct CommandEntry {
    std::string_view name;

    std::string_view description;

    int (*run)(std::span<std::string> args) = nullptr;
};

inline auto& command_entries() {
    static std::array<CommandEntry, kota::meta::reflection<Command>::member_count> entries;
    return entries;
}

/// Registers a subcommand, from the initializer of a variable at namespace scope of its file.
inline bool register_command(Command command, CommandEntry entry) {
    auto& registered = command_entries()[std::to_underlying(command)];
    assert(registered.run == nullptr && "a subcommand registers once");
    registered = entry;
    return true;
}

}  // namespace catter::driver
