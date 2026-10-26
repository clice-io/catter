module;

#include "modules/prelude.h"

module catter;

import catter.command;
import :protocol.analysis;

namespace catter {

namespace {

/// Whether `value` is the name of a value of `E`.
template <typename E>
bool names_one(std::string_view value) {
    return std::ranges::any_of(kota::meta::reflection<E>::member_values,
                               [&](E each) { return name(each) == value; });
}

std::error_code read_absolute(kota::buffer_ref out, std::string_view path) {
    if(!kota::fs::path::is_absolute(path)) {
        return std::make_error_code(std::errc::no_such_file_or_directory);
    }
    return kota::fs::read_file(out, path);
}

bool lists(const std::optional<std::vector<std::string>>& names, std::string_view name) {
    return !names.has_value() || std::ranges::contains(*names, name);
}

std::vector<protocol::Argument> arguments_of(std::span<const Argument> arguments, Syntax syntax) {
    std::vector<protocol::Argument> messages;
    messages.reserve(arguments.size());
    for(const auto& argument: arguments) {
        messages.push_back({
            .option = std::string(option_name(syntax, argument.id)),
            .spelling = protocol::to_os_string(argument.spelling),
            .index = argument.index,
            .count = argument.count,
            .values = protocol::to_os_string(argument.values),
        });
    }
    return messages;
}

protocol::CompilerAnalysis message_of(const CompilerAnalysis& analysis) {
    const auto& target = analysis.target;
    auto name_of = [](const auto& value) {
        return value.has_value() ? std::optional(std::string(name(*value))) : std::nullopt;
    };
    protocol::CompilerAnalysis message{
        .compiler = std::string(name(analysis.identity.compiler)),
        .dialect = std::string(name(analysis.dialect)),
        .phase = std::string(name(analysis.phase)),
        .artifact = std::string(name(analysis.artifact)),
        .target =
            {
                     .triple = target.triple,
                     .os = name_of(target.os),
                     .object_format = name_of(target.object_format),
                     .env = name_of(target.env),
                     .artifact_model = std::string(name(target.model)),
                     .source = std::string(name(target.source)),
                     },
        .arguments = arguments_of(analysis.arguments, syntax_of(analysis.dialect)),
    };
    if(!analysis.expansion.files.empty()) {
        message.expansion = protocol::Expansion{
            .files = protocol::to_os_string(analysis.expansion.files),
            .args = protocol::to_os_string(analysis.expansion.argv),
        };
    }
    for(const auto& read: analysis.reads) {
        message.reads.push_back({
            .path = protocol::to_os_string(read.path),
            .argument = read.location.argument,
            .index = read.location.index,
            .role = std::string(name(read.role)),
            .language = read.language,
        });
    }
    for(const auto& write: analysis.writes) {
        message.writes.push_back({
            .path = protocol::to_os_string(write.path),
            .reads = write.reads,
            .origin = std::string(name(write.origin)),
        });
    }
    for(const auto& diagnostic: analysis.diagnostics) {
        auto& added = message.diagnostics.emplace_back(protocol::Diagnostic{
            .code = std::string(name(diagnostic.code)),
            .message = kota::replace_invalid_utf8(diagnostic.message),
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

CommandAnalysis::CommandAnalysis(std::string_view executable,
                                 std::span<const std::string> argv,
                                 std::optional<std::string_view> cwd) {
    if(auto identity = identify(executable)) {
        auto compiler =
            analyze_compiler(executable,
                             argv,
                             cwd.value_or(""),
                             cwd ? ReadFile(kota::fs::read_file) : ReadFile(read_absolute));
        if(compiler.has_value()) {
            result = std::move(*compiler);
        } else {
            result = Unparsed{
                .of = "compiler",
                .name = std::string(name(identity->compiler)),
                .error = std::move(compiler.error()),
            };
        }
        return;
    }
    auto archiver = analyze_archiver(executable, argv);
    if(archiver.has_value()) {
        result = std::move(*archiver);
        return;
    }
    if(archiver.error().kind != Error::Kind::ArchiverNotRecognized) {
        result = Unparsed{.of = "archiver", .name = "ar", .error = std::move(archiver.error())};
        return;
    }
    if(auto tool = identify_tool(executable)) {
        auto analysis = analyze_tool(executable, argv);
        if(analysis.has_value()) {
            result = std::move(*analysis);
        } else {
            result = Unparsed{
                .of = "tool",
                .name = std::string(name(*tool)),
                .error = std::move(analysis.error()),
            };
        }
    }
}

bool CommandAnalysis::matches(std::span<const protocol::Pattern> filter) const {
    return std::ranges::any_of(filter, [&](const protocol::Pattern& pattern) {
        if(auto* compiler = std::get_if<CompilerAnalysis>(&result)) {
            return pattern.kind == "compiler" &&
                   lists(pattern.names, name(compiler->identity.compiler)) &&
                   lists(pattern.phases, name(compiler->phase));
        }
        if(std::holds_alternative<ArchiverAnalysis>(result)) {
            return pattern.kind == "archiver";
        }
        if(auto* tool = std::get_if<ToolAnalysis>(&result)) {
            return pattern.kind == "tool" && lists(pattern.names, name(tool->tool));
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
    if(auto* compiler = std::get_if<CompilerAnalysis>(&result)) {
        return message_of(*compiler);
    }
    if(auto* archiver = std::get_if<ArchiverAnalysis>(&result)) {
        return protocol::ArchiverAnalysis{
            .operation = std::string(name(archiver->operation)),
            .modifiers = kota::replace_invalid_utf8(archiver->modifiers),
            .thin = archiver->thin,
            .archive = protocol::to_os_string(archiver->archive),
            .members = protocol::to_os_string(archiver->members),
            .reads = protocol::to_os_string(archiver->reads),
            .writes = protocol::to_os_string(archiver->writes),
        };
    }
    if(auto* tool = std::get_if<ToolAnalysis>(&result)) {
        return protocol::ToolAnalysis{
            .tool = std::string(name(tool->tool)),
            .arguments = arguments_of(tool->arguments, syntax_of(tool->tool)),
        };
    }
    if(auto* unparsed = std::get_if<Unparsed>(&result)) {
        return protocol::UnparsedAnalysis{
            .of = std::string(unparsed->of),
            .name = unparsed->name,
            .error = std::string(name(unparsed->error.kind)),
            .message = kota::replace_invalid_utf8(unparsed->error.message),
        };
    }
    return std::nullopt;
}

std::expected<std::vector<std::string>, std::string>
    CommandAnalysis::edit(std::span<const std::string> argv,
                          std::span<const protocol::CommandEdit> edits) const {
    auto* compiler = std::get_if<CompilerAnalysis>(&result);
    auto* tool = std::get_if<ToolAnalysis>(&result);
    if(compiler == nullptr && tool == nullptr) {
        return std::unexpected(
            "catter reads no arguments of the command to edit: give it whole with `modify`");
    }
    std::vector<CommandEdit> changes;
    for(const auto& edit: edits) {
        CommandEdit change;
        if(edit.kind == "remove") {
            change.kind = CommandEdit::Kind::Remove;
        } else if(edit.kind == "append") {
            change.kind = CommandEdit::Kind::Append;
        } else if(edit.kind == "prepend") {
            change.kind = CommandEdit::Kind::Prepend;
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
    if(compiler != nullptr) {
        return apply_edits(argv,
                           compiler->expansion,
                           compiler->arguments,
                           syntax_of(compiler->dialect),
                           changes);
    }
    return apply_edits(argv, tool->arguments, syntax_of(tool->tool), changes);
}

std::optional<std::string> check_filter(std::span<const protocol::Pattern> filter) {
    for(const auto& pattern: filter) {
        const auto names = pattern.names.value_or(std::vector<std::string>{});
        if(pattern.kind == "compiler") {
            for(const auto& name: names) {
                if(!names_one<Compiler>(name)) {
                    return std::format("`{}` is no compiler catter knows", name);
                }
            }
            for(const auto& phase: pattern.phases.value_or(std::vector<std::string>{})) {
                if(!names_one<Phase>(phase)) {
                    return std::format("`{}` is no phase of a compiler", phase);
                }
            }
        } else if(pattern.kind == "tool") {
            for(const auto& name: names) {
                if(!names_one<Tool>(name)) {
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

}  // namespace catter
