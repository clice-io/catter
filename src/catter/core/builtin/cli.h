#pragma once

#include <cstdint>
#include <format>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <cpptrace/exceptions.hpp>
#include <kota/deco/deco.h>

namespace catter::builtin {

struct Example {
    std::string_view command;

    std::string_view description = {};
};

/// What the help of a built-in says besides its options: its name, what it does, examples.
struct Help {
    std::string_view name;

    std::string_view description;

    std::vector<Example> examples = {};
};

/// The text renderer of a built-in's command line: its help laid out as the built-ins have
/// always laid it out, `<name>: <message>` for an error.
kota::deco::cli::text::Renderer renderer_of(const Help& help);

/// The help of the built-in whose options `T` declares.
template <typename T>
std::string usage_of(const Help& help) {
    auto command = kota::deco::cli::command<T>(help.name);
    command.render_with(renderer_of(help));
    std::ostringstream usage;
    command.usage(usage);
    return usage.str();
}

/// The options `args` gives a built-in, read as `T` declares them; none when they ask for its
/// help, which is printed. `T` declares the help flag, `-h, --help`, last, and at most one
/// positional argument, an optional one.
///
/// Throws for an argument the built-in does not take, with its help.
template <typename T>
std::optional<T> parse_options(const Help& help, std::span<const std::string> args) {
    std::vector<std::string> argv(args.begin(), args.end());
    bool asked_for_help = false;
    auto command = kota::deco::cli::command<T>(help.name);
    command.render_with(renderer_of(help)).template after<&T::help>([&](auto& step) {
        asked_for_help = true;
        return step.stop();
    });
    auto result = command.invoke(argv);
    if(asked_for_help) {
        std::println("{}", usage_of<T>(help));
        return std::nullopt;
    }
    if(!result.has_value()) {
        throw cpptrace::runtime_error(
            std::format("{}\n\n{}", result.error().message, usage_of<T>(help)));
    }
    // deco keeps the last of several inputs for one; a built-in takes one at most.
    auto inputs = result->parsed_arguments | std::views::filter([](const auto& argument) {
                      return !argument.spelling.starts_with('-');
                  });
    if(std::ranges::distance(inputs) > 1) {
        throw cpptrace::runtime_error(std::format("{}: unexpected input argument {}\n\n{}",
                                                  help.name,
                                                  std::ranges::next(inputs.begin())->spelling,
                                                  usage_of<T>(help)));
    }
    return std::move(result->options);
}

/// Throws for a value of `option` below `least`, with the help of the built-in whose options
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

}  // namespace catter::builtin
