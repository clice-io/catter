module;

#include "modules/prelude.h"

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

/// The text renderer of a subcommand's command line: its help laid out as `catter <name>
/// --help` prints it, `<name>: <message>` for an error.
kota::deco::cli::text::Renderer renderer_of(const Help& help);

/// The help of the subcommand whose options `T` declares.
template <typename T>
std::string usage_of(const Help& help) {
    auto command = kota::deco::cli::command<T>(help.name);
    command.render_with(renderer_of(help));
    std::ostringstream usage;
    command.usage(usage);
    return usage.str();
}

/// The options `args` gives a subcommand, read as `T` declares them; none when they ask for
/// its help, which is printed. `T` declares the help flag, `-h, --help`, last, and, when the
/// subcommand runs a build, the session's options as `session`.
///
/// Throws for an argument the subcommand does not take, with its help.
template <typename T>
std::optional<T> parse(const Help& help, std::span<const std::string> args) {
    std::vector<std::string> argv(args.begin(), args.end());
    bool asked_for_help = false;
    auto command = kota::deco::cli::command<T>(help.name);
    command.render_with(renderer_of(help)).template after<&T::help>([&](auto& step) {
        asked_for_help = true;
        return step.stop();
    });
    if constexpr(requires { &T::session; }) {
        command
            .template after<&T::session, &SessionOptions::handler>([](auto& step) {
                step.options().session.handlers.push_back({.connect = false, .text = step.value()});
                return step.next();
            })
            .template after<&T::session, &SessionOptions::connect>([](auto& step) {
                step.options().session.handlers.push_back({.connect = true, .text = step.value()});
                return step.next();
            });
    }
    auto result = command.invoke(argv);
    if(asked_for_help) {
        std::println("{}", usage_of<T>(help));
        return std::nullopt;
    }
    if(!result.has_value()) {
        throw cpptrace::runtime_error(
            std::format("{}\n\n{}", result.error().message, usage_of<T>(help)));
    }
    return std::move(result->options);
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
                                                  usage_of<T>(help)));
    }
}

}  // namespace catter::driver
