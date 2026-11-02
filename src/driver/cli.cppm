module;

#include "support/prelude.h"

module catter:driver.cli;

import :driver.option;

namespace catter::driver {

/// The subcommands, in the order `catter --help` lists them.
enum class Subcommand : std::uint8_t {
    CompileCommands,
    Tree,
    Targets,
    Profile,
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
    static std::array<SubcommandEntry, meta::reflection<Subcommand>::member_count> entries;
    return entries;
}

/// Registers a subcommand, from the initializer of a variable at namespace scope of its file.
inline bool register_subcommand(Subcommand subcommand, SubcommandEntry entry) {
    auto& registered = subcommand_entries()[std::to_underlying(subcommand)];
    assert(registered.run == nullptr && "a subcommand registers once");
    registered = entry;
    return true;
}

struct Example {
    std::string_view command;

    std::string_view description = {};
};

/// What the help of a subcommand says besides its options: its name, what it does, examples.
struct Help {
    std::string_view name;

    std::string_view description;

    std::vector<Example> examples = {};
};

/// What `catter --help` prints: what catter does, and its subcommands.
std::string usage_of_catter();

/// The help option every subcommand declares, a `bool help` field.
KANNOTATION(help_option, cli::help, description = "Print this help message.");

/// The help of a subcommand, its arguments and options laid out from `entries`.
std::string usage_of(const Help& help, std::span<const cli::UsageEntry> entries);

/// The options `args` gives a subcommand, read as `T` declares them; none when they ask for
/// its help, which is printed. `T` declares its help option last, and, when the subcommand
/// runs a build, the session's options as `session`.
///
/// Throws for an argument the subcommand does not take, with its help.
template <typename T>
std::optional<T> parse(const Help& help, std::span<const std::string> args) {
    auto options = cli::parse<T>(args, [](T& parsed, const cli::Given& given) {
        if constexpr(requires { &T::session; }) {
            const bool connect = given.path == "session.connect";
            if(connect || given.path == "session.handler") {
                parsed.session.handlers.push_back(
                    {.connect = connect, .text = std::string(given.values.front())});
            }
        }
    });
    if(!options.has_value()) {
        throw cpptrace::runtime_error(std::format("{}: {}\n\n{}",
                                                  help.name,
                                                  options.error().message,
                                                  usage_of(help, cli::usage_entries<T>())));
    }
    if(options->help) {
        std::println("{}", usage_of(help, cli::usage_entries<T>()));
        return std::nullopt;
    }
    return std::move(*options);
}

/// Throws for a value of `option` below `least`, with the help of the subcommand whose options
/// `T` declares.
template <typename T>
void check_at_least(const Help& help,
                    std::string_view option,
                    std::optional<std::int64_t> value,
                    std::int64_t least) {
    if(value.has_value() && *value < least) {
        throw cpptrace::runtime_error(std::format("{}: expected {} >= {}, got {}\n\n{}",
                                                  help.name,
                                                  option,
                                                  least,
                                                  *value,
                                                  usage_of(help, cli::usage_entries<T>())));
    }
}

}  // namespace catter::driver
