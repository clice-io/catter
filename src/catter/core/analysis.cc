#include "analysis.h"

#include <algorithm>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include <kota/meta/enum.h>

#include "cmd/edit.h"
#include "cmd/syntax.h"

namespace catter::core {

namespace {

/// Whether `value` is the name of a value of `E`.
template <typename E>
bool names_one(std::string_view value) {
    return std::ranges::any_of(kota::meta::reflection<E>::member_values,
                               [&](E each) { return cmd::name(each) == value; });
}

bool lists(const std::optional<std::vector<std::string>>& names, std::string_view name) {
    return !names.has_value() || std::ranges::contains(*names, name);
}

std::vector<protocol::Argument> arguments_of(std::span<const cmd::Argument> arguments,
                                             cmd::Syntax syntax) {
    std::vector<protocol::Argument> messages;
    messages.reserve(arguments.size());
    for(const auto& argument: arguments) {
        messages.push_back({
            .option = std::string(cmd::option_name(syntax, argument.id)),
            .spelling = protocol::to_os_string(argument.spelling),
            .index = argument.index,
            .count = argument.count,
            .values = protocol::to_os_string(argument.values),
        });
    }
    return messages;
}

protocol::CompilerAnalysis message_of(const cmd::CompilerAnalysis& analysis) {
    const auto& target = analysis.target;
    auto name_of = [](const auto& value) {
        return value.has_value() ? std::optional(std::string(cmd::name(*value))) : std::nullopt;
    };
    protocol::CompilerAnalysis message{
        .compiler = std::string(cmd::name(analysis.identity.compiler)),
        .dialect = std::string(cmd::name(analysis.dialect)),
        .phase = std::string(cmd::name(analysis.phase)),
        .artifact = std::string(cmd::name(analysis.artifact)),
        .target =
            {
                     .triple = target.triple,
                     .os = name_of(target.os),
                     .object_format = name_of(target.object_format),
                     .env = name_of(target.env),
                     .artifact_model = std::string(cmd::name(target.model)),
                     .source = std::string(cmd::name(target.source)),
                     },
        .arguments = arguments_of(analysis.arguments, cmd::syntax_of(analysis.dialect)),
    };
    for(const auto& read: analysis.reads) {
        message.reads.push_back({
            .path = protocol::to_os_string(read.path),
            .argument = read.location.argument,
            .index = read.location.index,
            .role = std::string(cmd::name(read.role)),
            .language = read.language,
        });
    }
    for(const auto& write: analysis.writes) {
        message.writes.push_back({
            .path = protocol::to_os_string(write.path),
            .reads = write.reads,
            .origin = std::string(cmd::name(write.origin)),
        });
    }
    for(const auto& diagnostic: analysis.diagnostics) {
        auto& added = message.diagnostics.emplace_back(protocol::Diagnostic{
            .code = std::string(cmd::name(diagnostic.code)),
            .message = protocol::to_text(diagnostic.message),
        });
        if(diagnostic.path.has_value()) {
            added.path = protocol::to_os_string(*diagnostic.path);
        }
        if(diagnostic.location.has_value()) {
            added.argument = diagnostic.location->argument;
            added.index = diagnostic.location->index;
        }
    }
    return message;
}

}  // namespace

CommandAnalysis::CommandAnalysis(std::string_view executable, std::span<const std::string> argv) {
    if(auto identity = cmd::identify(executable)) {
        auto compiler = cmd::analyze_compiler(executable, argv);
        if(compiler.has_value()) {
            result = std::move(*compiler);
        } else {
            result = Unparsed{
                .of = "compiler",
                .name = std::string(cmd::name(identity->compiler)),
                .error = std::move(compiler.error()),
            };
        }
        return;
    }
    auto archiver = cmd::analyze_archiver(executable, argv);
    if(archiver.has_value()) {
        result = std::move(*archiver);
        return;
    }
    if(archiver.error().kind != cmd::Error::Kind::ArchiverNotRecognized) {
        result = Unparsed{.of = "archiver", .name = "ar", .error = std::move(archiver.error())};
        return;
    }
    if(auto tool = cmd::identify_tool(executable)) {
        auto analysis = cmd::analyze_tool(executable, argv);
        if(analysis.has_value()) {
            result = std::move(*analysis);
        } else {
            result = Unparsed{
                .of = "tool",
                .name = std::string(cmd::name(*tool)),
                .error = std::move(analysis.error()),
            };
        }
    }
}

bool CommandAnalysis::matches(std::span<const protocol::Pattern> filter) const {
    return std::ranges::any_of(filter, [&](const protocol::Pattern& pattern) {
        if(auto* compiler = std::get_if<cmd::CompilerAnalysis>(&result)) {
            return pattern.kind == "compiler" &&
                   lists(pattern.names, cmd::name(compiler->identity.compiler)) &&
                   lists(pattern.phases, cmd::name(compiler->phase));
        }
        if(std::holds_alternative<cmd::ArchiverAnalysis>(result)) {
            return pattern.kind == "archiver";
        }
        if(auto* tool = std::get_if<cmd::ToolAnalysis>(&result)) {
            return pattern.kind == "tool" && lists(pattern.names, cmd::name(tool->tool));
        }
        if(auto* unparsed = std::get_if<Unparsed>(&result)) {
            // A command whose arguments do not read has no phase.
            return pattern.kind == unparsed->of && lists(pattern.names, unparsed->name) &&
                   !pattern.phases.has_value();
        }
        return false;
    });
}

std::optional<protocol::Analysis> CommandAnalysis::message() const {
    if(auto* compiler = std::get_if<cmd::CompilerAnalysis>(&result)) {
        return message_of(*compiler);
    }
    if(auto* archiver = std::get_if<cmd::ArchiverAnalysis>(&result)) {
        return protocol::ArchiverAnalysis{
            .operation = std::string(cmd::name(archiver->operation)),
            .modifiers = protocol::to_text(archiver->modifiers),
            .thin = archiver->thin,
            .archive = protocol::to_os_string(archiver->archive),
            .members = protocol::to_os_string(archiver->members),
            .reads = protocol::to_os_string(archiver->reads),
            .writes = protocol::to_os_string(archiver->writes),
        };
    }
    if(auto* tool = std::get_if<cmd::ToolAnalysis>(&result)) {
        return protocol::ToolAnalysis{
            .tool = std::string(cmd::name(tool->tool)),
            .arguments = arguments_of(tool->arguments, cmd::syntax_of(tool->tool)),
        };
    }
    if(auto* unparsed = std::get_if<Unparsed>(&result)) {
        return protocol::UnparsedAnalysis{
            .of = std::string(unparsed->of),
            .name = unparsed->name,
            .error = std::string(cmd::name(unparsed->error.kind)),
            .message = protocol::to_text(unparsed->error.message),
        };
    }
    return std::nullopt;
}

std::expected<std::vector<std::string>, std::string>
    CommandAnalysis::edit(std::span<const std::string> argv,
                          std::span<const protocol::CommandEdit> edits) const {
    std::optional<cmd::Syntax> syntax;
    std::span<const cmd::Argument> arguments;
    if(auto* compiler = std::get_if<cmd::CompilerAnalysis>(&result)) {
        syntax = cmd::syntax_of(compiler->dialect);
        arguments = compiler->arguments;
    } else if(auto* tool = std::get_if<cmd::ToolAnalysis>(&result)) {
        syntax = cmd::syntax_of(tool->tool);
        arguments = tool->arguments;
    } else {
        return std::unexpected(
            "catter reads no arguments of the command to edit: give it whole with `modify`");
    }
    std::vector<cmd::CommandEdit> changes;
    for(const auto& edit: edits) {
        cmd::CommandEdit change;
        if(edit.kind == "remove") {
            change.kind = cmd::CommandEdit::Kind::Remove;
        } else if(edit.kind == "append") {
            change.kind = cmd::CommandEdit::Kind::Append;
        } else if(edit.kind == "prepend") {
            change.kind = cmd::CommandEdit::Kind::Prepend;
        } else {
            return std::unexpected(
                std::format("`{}` is no kind of edit: give `remove`, `append` or `prepend`",
                            edit.kind));
        }
        auto flags = protocol::to_bytes(edit.flags);
        if(!flags.has_value()) {
            return std::unexpected(std::move(flags.error()));
        }
        change.flags = std::move(*flags);
        changes.push_back(std::move(change));
    }
    return cmd::apply_edits(argv, arguments, *syntax, changes);
}

std::optional<std::string> check_filter(std::span<const protocol::Pattern> filter) {
    for(const auto& pattern: filter) {
        const auto names = pattern.names.value_or(std::vector<std::string>{});
        if(pattern.kind == "compiler") {
            for(const auto& name: names) {
                if(!names_one<cmd::Compiler>(name)) {
                    return std::format("`{}` is no compiler catter knows", name);
                }
            }
            for(const auto& phase: pattern.phases.value_or(std::vector<std::string>{})) {
                if(!names_one<cmd::Phase>(phase)) {
                    return std::format("`{}` is no phase of a compiler", phase);
                }
            }
        } else if(pattern.kind == "tool") {
            for(const auto& name: names) {
                if(!names_one<cmd::Tool>(name)) {
                    return std::format("`{}` is no tool catter knows", name);
                }
            }
            if(pattern.phases.has_value()) {
                return "a pattern of tools names no phases";
            }
        } else if(pattern.kind == "archiver") {
            if(pattern.names.has_value() || pattern.phases.has_value()) {
                return "a pattern of archivers names no programs and no phases";
            }
        } else {
            return std::format(
                "`{}` is no kind of program catter analyzes: give `compiler`, `archiver` or `tool`",
                pattern.kind);
        }
    }
    return std::nullopt;
}

}  // namespace catter::core
