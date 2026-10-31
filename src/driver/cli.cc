module;

#include "support/prelude.h"

module catter;

import :driver.cli;

namespace catter::driver {

namespace {

namespace text = deco::cli::text;

using Row = std::pair<std::string, std::string>;

/// The rows of a section, a label and its description each, the descriptions in one column.
std::string columns(std::span<const Row> rows) {
    std::size_t width = 0;
    for(const auto& [label, description]: rows) {
        width = std::max(width, label.size());
    }
    std::vector<std::string> lines;
    for(const auto& [label, description]: rows) {
        lines.push_back(description.empty()
                            ? std::format("  {}", label)
                            : std::format("  {:<{}}  {}", label, width, description));
    }
    return lines | views::join_with('\n') | ranges::to<std::string>();
}

/// `-o|--output <path>|--output=<path>`, deco's usage of an option, as `-o, --output <path>`:
/// its forms with the value joined are left out.
std::string aliases(std::string_view usage) {
    return usage | views::split('|') |
           views::filter([](auto form) { return !std::string_view(form).contains('='); }) |
           views::join_with(std::string_view(", ")) | ranges::to<std::string>();
}

std::string layout(const Help& help, const text::UsageDocument& document) {
    std::vector<Row> arguments;
    std::vector<Row> options;
    for(const auto& group: document.groups) {
        for(const auto& entry: group.entries) {
            if(entry.usage.starts_with("-- ")) {
                arguments.emplace_back(entry.usage, entry.help);
            } else {
                options.emplace_back(aliases(entry.usage), entry.help);
            }
        }
    }

    std::string synopsis = std::format("catter {} [options]", help.name);
    for(const auto& [label, description]: arguments) {
        synopsis += " " + label;
    }
    std::vector<std::string> sections = {
        std::string(help.description),
        std::format("Usage:\n  {}", synopsis),
    };
    if(!arguments.empty()) {
        sections.push_back(std::format("Arguments:\n{}", columns(arguments)));
    }
    sections.push_back(std::format("Options:\n{}", columns(options)));
    if(!help.examples.empty()) {
        std::string examples = "Examples:";
        for(const auto& example: help.examples) {
            examples += std::format("\n  {}", example.command);
            if(!example.description.empty()) {
                examples += std::format("\n  # {}", example.description);
            }
        }
        sections.push_back(std::move(examples));
    }
    return sections | views::join_with(std::string_view("\n\n")) | ranges::to<std::string>();
}

}  // namespace

std::string usage_of_catter() {
    std::vector<Row> commands;
    for(const auto& entry: subcommand_entries()) {
        commands.emplace_back(std::string(entry.name), std::string(entry.description));
    }
    std::vector<std::string> sections = {
        "catter runs a build, captures every command it starts, and analyzes them.",
        "Usage:\n  catter <command> [options] -- <build command>...",
        std::format("Commands:\n{}", columns(commands)),
        "Run `catter <command> --help` for the options of a command.",
    };
    return sections | views::join_with(std::string_view("\n\n")) | ranges::to<std::string>();
}

text::Renderer renderer_of(const Help& help) {
    text::Renderer renderer;
    renderer.usage = [help](const text::UsageDocument& document, bool, const text::TextStyle&) {
        return layout(help, document);
    };
    renderer.diagnostic = [name = help.name](const text::Diagnostic& diagnostic,
                                             const text::TextStyle&) {
        return std::format("{}: {}", name, diagnostic.message);
    };
    return renderer;
}

}  // namespace catter::driver
