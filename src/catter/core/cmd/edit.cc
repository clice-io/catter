#include "cmd/edit.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cmd/lowercase.h"

namespace catter::cmd {

namespace {

/// An argument with the elements of argv it is written as; or an element of argv no argument
/// holds, `--`.
struct Piece {
    std::optional<Argument> argument;

    std::vector<std::string> elements;
};

/// What a flag of a remove names.
struct RemovePattern {
    Argument option;

    /// `-W*`: every option in the option's group whose name starts with the option's
    /// spelling.
    bool prefix = false;
};

/// The arguments of `flags` with the flags each is written as.
std::expected<std::vector<Piece>, std::string> read_flags(Syntax syntax,
                                                          std::span<const std::string> flags) {
    std::vector<std::string> argv{""};
    argv.insert(argv.end(), flags.begin(), flags.end());
    auto arguments = read_arguments(syntax, argv);
    if(!arguments.has_value()) {
        return std::unexpected(std::move(arguments.error()));
    }
    std::vector<Piece> pieces;
    for(const auto& argument: *arguments) {
        auto first = argv.begin() + argument.index;
        pieces.push_back({
            .argument = argument,
            .elements = {first, first + argument.count}
        });
    }
    return pieces;
}

std::expected<std::vector<RemovePattern>, std::string>
    read_patterns(Syntax syntax, std::span<const std::string> flags) {
    auto pieces = read_flags(syntax, flags);
    if(!pieces.has_value() && !flags.empty()) {
        // An option written last without its value, `-include`, names it with any value.
        std::vector<std::string> wildcard(flags.begin(), flags.end());
        wildcard.emplace_back("*");
        auto retried = read_flags(syntax, wildcard);
        if(retried.has_value() && retried->back().argument->index == flags.size()) {
            pieces = std::move(retried);
        }
    }
    if(!pieces.has_value()) {
        return std::unexpected(std::format("the flags to remove do not parse: {}", pieces.error()));
    }
    std::vector<RemovePattern> patterns;
    for(auto& piece: *pieces) {
        auto& option = *piece.argument;
        if(option.id == syntax.table->input_option_id) {
            return std::unexpected(
                std::format("`{}` names no option to remove", piece.elements.front()));
        }
        std::string_view written = piece.elements.front();
        const bool prefix =
            written.ends_with('*') && written.substr(0, written.size() - 1) == option.spelling;
        patterns.push_back({.option = std::move(option), .prefix = prefix});
    }
    return patterns;
}

bool removes(const RemovePattern& pattern, const Piece& piece, Syntax syntax) {
    if(!piece.argument.has_value()) {
        return false;
    }
    const auto& argument = *piece.argument;
    const auto& wanted = pattern.option;
    if(pattern.prefix) {
        // By the argument's name, which an alias of its option does not change, with the
        // value it joins: `-Wall` of `/W3`.
        auto group = syntax.table->option(wanted.id)->group();
        auto name = std::string(option_name(syntax, argument.id));
        if(!argument.values.empty()) {
            name += argument.values.front();
        }
        if(group.has_value() && syntax.table->option(argument.id)->matches(group->id()) &&
           name.starts_with(wanted.spelling)) {
            return true;
        }
    }
    if(argument.id != wanted.id) {
        return false;
    }
    if(argument.id == syntax.table->unknown_option_id) {
        // An unknown option is its spelling; `=*` after it takes any value.
        std::string_view spelling = wanted.spelling;
        if(spelling.ends_with("=*")) {
            return argument.spelling.starts_with(spelling.substr(0, spelling.size() - 1));
        }
        return argument.spelling == spelling;
    }
    if(wanted.values.size() == 1 && wanted.values[0] == "*") {
        return true;
    }
    return argument.values == wanted.values;
}

/// The pieces of `argv`, but argv[0], in order.
std::vector<Piece> pieces_of(std::span<const std::string> argv,
                             std::span<const Argument> arguments) {
    std::vector<Piece> pieces;
    std::size_t next = 1;
    auto gaps_to = [&](std::size_t end) {
        for(; next < end; next += 1) {
            pieces.push_back({.elements = {argv[next]}});
        }
    };
    for(const auto& argument: arguments) {
        gaps_to(argument.index);
        auto first = argv.begin() + argument.index;
        pieces.push_back({
            .argument = argument,
            .elements = {first, first + argument.count}
        });
        next = argument.index + argument.count;
    }
    gaps_to(argv.size());
    return pieces;
}

/// Where appended flags go among `pieces`: before what ends the options, `--` or `/link`, or
/// at the end.
std::size_t end_of_options(std::span<const Piece> pieces, Syntax syntax) {
    auto end = std::ranges::find_if(pieces, [&](const Piece& piece) {
        if(!piece.argument.has_value()) {
            return piece.elements.front() == "--";
        }
        return lowercase(option_name(syntax, piece.argument->id)) == "/link";
    });
    return static_cast<std::size_t>(end - pieces.begin());
}

}  // namespace

std::expected<std::vector<std::string>, std::string>
    apply_edits(std::span<const std::string> argv,
                std::span<const Argument> arguments,
                Syntax syntax,
                std::span<const CommandEdit> edits) {
    if(argv.empty()) {
        return std::unexpected("the command has no program to edit the arguments of");
    }
    auto base = pieces_of(argv, arguments);
    std::vector<bool> removed(base.size());
    std::vector<Piece> prepended;
    std::vector<Piece> appended;
    for(const auto& edit: edits) {
        if(edit.kind == CommandEdit::Kind::Remove) {
            auto patterns = read_patterns(syntax, edit.flags);
            if(!patterns.has_value()) {
                return std::unexpected(std::move(patterns.error()));
            }
            auto named = [&](const Piece& piece) {
                return std::ranges::any_of(*patterns, [&](const RemovePattern& pattern) {
                    return removes(pattern, piece, syntax);
                });
            };
            for(std::size_t i = 0; i < base.size(); i += 1) {
                removed[i] = removed[i] || named(base[i]);
            }
            std::erase_if(prepended, named);
            std::erase_if(appended, named);
            continue;
        }
        auto pieces = read_flags(syntax, edit.flags);
        if(!pieces.has_value()) {
            return std::unexpected(
                std::format("the flags to add do not parse: {}", pieces.error()));
        }
        auto& into = edit.kind == CommandEdit::Kind::Append ? appended : prepended;
        into.insert(into.end(), pieces->begin(), pieces->end());
    }

    std::vector<std::string> edited{argv.front()};
    auto add = [&](const Piece& piece) {
        edited.insert(edited.end(), piece.elements.begin(), piece.elements.end());
    };
    std::ranges::for_each(prepended, add);
    const auto slot = end_of_options(base, syntax);
    for(std::size_t i = 0; i < base.size(); i += 1) {
        if(i == slot) {
            std::ranges::for_each(appended, add);
        }
        if(!removed[i]) {
            add(base[i]);
        }
    }
    if(slot == base.size()) {
        std::ranges::for_each(appended, add);
    }
    return edited;
}

}  // namespace catter::cmd
