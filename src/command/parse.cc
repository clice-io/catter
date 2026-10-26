module;

#include "modules/prelude.h"

module catter.command;

import :parse;

namespace catter {

namespace {

using namespace option::clang;

std::expected<std::vector<Argument>, Error> read_arguments(std::span<const std::string> argv,
                                                           Dialect dialect) {
    auto arguments = read_arguments(syntax_of(dialect), argv);
    if(!arguments.has_value()) {
        return std::unexpected(Error{
            .kind = Error::Kind::CompilerParse,
            .message = std::move(arguments.error()),
        });
    }
    return std::move(*arguments);
}

/// Whether `argument` is one of clang's Action_Group options, which select the mode.
bool selects_mode(const Argument& argument) {
    auto group = table().option(argument.id)->group();
    return group.has_value() && group->id() == ID_Action_Group;
}

/// Builds the parsed command from its arguments, one at a time, and the phase and artifact
/// from the options that select the mode, in the order they come.
struct Builder {
    enum class LinkPlan : std::uint8_t {
        Executable,
        SharedLibrary,
        StaticLibrary,
        RelocatableObject,
    };

    ParsedCommand command;

    bool preprocess = false;

    bool syntax_only = false;

    std::optional<Artifact> compiled;

    bool assembly_like = false;

    bool llvm_like = false;

    LinkPlan plan = LinkPlan::Executable;

    /// The language `-x` sets for the inputs after it.
    std::optional<std::string> language;

    /// The language `/TC` or `/TP` sets for every input.
    std::optional<std::string> every_language;

    void compile_object() {
        if(!compiled.has_value() || compiled == Artifact::Unknown) {
            compiled = Artifact::Object;
        }
    }

    void compile_assembly_like() {
        assembly_like = true;
        compiled = llvm_like ? Artifact::LLVMIR : Artifact::Assembly;
    }

    void compile_llvm_like() {
        llvm_like = true;
        compiled = assembly_like ? Artifact::LLVMIR : Artifact::LLVMBitcode;
    }

    void compile_unknown() {
        if(!compiled.has_value()) {
            compiled = Artifact::Unknown;
        }
    }

    void link_shared_library() {
        if(plan == LinkPlan::Executable) {
            plan = LinkPlan::SharedLibrary;
        }
    }

    void link(LinkPlan archive_or_relocatable) {
        if(plan == LinkPlan::Executable || plan == LinkPlan::SharedLibrary) {
            plan = archive_or_relocatable;
        }
    }

    /// The location of the value of option `position`, which takes one.
    Location value_location(std::uint32_t position) const {
        return {.argument = position, .index = command.arguments[position].value_index(0)};
    }

    void output(std::uint32_t position, OutputKind kind) {
        command.outputs.push_back(Output{
            .path = command.arguments[position].values[0],
            .location = value_location(position),
            .kind = kind,
        });
    }

    void candidate(std::uint32_t position) {
        const auto& argument = command.arguments[position];
        command.candidates.push_back(Candidate{
            .path = argument.spelling,
            .location = {.argument = position, .index = argument.index},
            .language = language,
        });
    }

    void apply_gnu(std::uint32_t position) {
        const auto& argument = command.arguments[position];
        switch(argument.id) {
            case ID_c:
            case ID_emit_obj: compile_object(); break;
            case ID_S: compile_assembly_like(); break;
            case ID_E: preprocess = true; break;
            case ID_fsyntax_only: syntax_only = true; break;
            case ID_emit_llvm:
            case ID_emit_llvm_bc: compile_llvm_like(); break;
            case ID_emit_pch: compiled = Artifact::PCH; break;
            case ID_emit_module:
            case ID_emit_module_interface:
            case ID_emit_reduced_module_interface: compiled = Artifact::PCM; break;
            case ID_emit_static_lib: link(LinkPlan::StaticLibrary); break;
            case ID_shared: link_shared_library(); break;
            case ID_r: link(LinkPlan::RelocatableObject); break;
            case ID_o: output(position, OutputKind::PrimaryArtifact); break;
            case ID_target:
            case ID_target_legacy_spelling: command.triple = argument.values[0]; break;
            case ID_x: language = argument.values[0]; break;
            case ID_driver_mode: command.cxx = argument.values[0] == "g++"; break;
            case ID_INPUT: candidate(position); break;
            default:
                if(selects_mode(argument)) {
                    compile_unknown();
                }
                break;
        }
    }

    std::optional<Error> apply_cl(std::uint32_t position) {
        const auto& argument = command.arguments[position];
        switch(argument.id) {
            case ID_c: compile_object(); break;
            case ID_E:
            case ID_P:
            case ID__SLASH_P: preprocess = true; break;
            case ID_fsyntax_only: syntax_only = true; break;
            case ID__SLASH_LD:
            case ID__SLASH_LDd: link_shared_library(); break;
            case ID_o:
            case ID__SLASH_o: output(position, OutputKind::PrimaryArtifact); break;
            case ID_target:
            case ID_target_legacy_spelling: command.triple = argument.values[0]; break;
            case ID_x: language = argument.values[0]; break;
            case ID__SLASH_Fo: output(position, OutputKind::ObjectFile); break;
            case ID__SLASH_Fe: output(position, OutputKind::LinkedArtifact); break;
            case ID__SLASH_FA: command.listing = true; break;
            case ID__SLASH_Fa:
                command.listing = true;
                // `/Fa` alone names each listing after its source, as `/FA` does.
                command.listing_path =
                    argument.values[0].empty() ? std::nullopt : std::optional(argument.values[0]);
                break;
            case ID__SLASH_TC: every_language = "c"; break;
            case ID__SLASH_TP: every_language = "c++"; break;
            case ID__SLASH_Tc:
            case ID__SLASH_Tp:
                command.inputs.push_back(Read{
                    .path = argument.values[0],
                    .location = value_location(position),
                    .role = InputRole::Source,
                    .language = argument.id == ID__SLASH_Tc ? "c" : "c++",
                });
                break;
            case ID__SLASH_link: apply_linker_arguments(position); break;
            case ID_INPUT: candidate(position); break;
            default:
                if(selects_mode(argument)) {
                    return Error{
                        .kind = Error::Kind::CompilerParse,
                        .message =
                            std::format("unsupported clang-cl action option {}", argument.spelling),
                    };
                }
                break;
        }
        return std::nullopt;
    }

    /// What the arguments after `/link`, which go to the linker, say about the files of the
    /// link: only `/dll`, `/out` and its inputs, until catter analyzes linkers of their own.
    void apply_linker_arguments(std::uint32_t position) {
        const auto& argument = command.arguments[position];
        const auto& values = argument.values;
        for(std::uint32_t i = 0; i < values.size(); i += 1) {
            const auto& token = values[i];
            const auto lower = lowercase(token);
            if(lower == "/dll") {
                link_shared_library();
            } else if(lower.starts_with("/out:")) {
                command.outputs.push_back(Output{
                    .path = token.substr(5),
                    .location = {.argument = position, .index = argument.value_index(i)},
                    .kind = OutputKind::LinkedArtifact,
                });
            } else if(lower == "/out" && i + 1 < values.size()) {
                i += 1;
                command.outputs.push_back(Output{
                    .path = values[i],
                    .location = {.argument = position, .index = argument.value_index(i)},
                    .kind = OutputKind::LinkedArtifact,
                });
            } else if(!token.starts_with('@') && !token.starts_with('-') &&
                      !token.starts_with('/')) {
                command.inputs.push_back(Read{
                    .path = token,
                    .location = {.argument = position, .index = argument.value_index(i)},
                    .role = InputRole::Link,
                });
            }
        }
    }

    void finish() {
        if(every_language.has_value()) {
            for(auto& candidate: command.candidates) {
                candidate.language = every_language;
            }
        }
        if(preprocess) {
            command.phase = Phase::Preprocess;
            command.artifact = Artifact::PreprocessedSource;
        } else if(syntax_only) {
            command.phase = Phase::SyntaxOnly;
            command.artifact = Artifact::None;
        } else if(compiled.has_value()) {
            command.phase = Phase::Compile;
            command.artifact = *compiled;
        } else {
            switch(plan) {
                case LinkPlan::Executable:
                    command.phase = Phase::Link;
                    command.artifact = Artifact::Executable;
                    break;
                case LinkPlan::SharedLibrary:
                    command.phase = Phase::Link;
                    command.artifact = Artifact::SharedLibrary;
                    break;
                case LinkPlan::StaticLibrary:
                    command.phase = Phase::Archive;
                    command.artifact = Artifact::StaticLibrary;
                    break;
                case LinkPlan::RelocatableObject:
                    command.phase = Phase::RelocatableLink;
                    command.artifact = Artifact::Object;
                    break;
            }
        }
    }
};

ParsedCommand parse_gnu(const Identity& identity, std::vector<Argument> arguments) {
    Builder builder{
        .command = {.dialect = identity.dialect,
                    .cxx = identity.cxx,
                    .arguments = std::move(arguments)}
    };
    for(std::uint32_t position = 0; position < builder.command.arguments.size(); position += 1) {
        builder.apply_gnu(position);
    }
    builder.finish();
    return std::move(builder.command);
}

std::expected<ParsedCommand, Error> parse_cl(std::span<const std::string> argv) {
    auto arguments = read_arguments(argv, Dialect::MSVC);
    if(!arguments.has_value()) {
        return std::unexpected(std::move(arguments.error()));
    }
    Builder builder{
        .command = {.dialect = Dialect::MSVC, .arguments = std::move(*arguments)}
    };
    for(std::uint32_t position = 0; position < builder.command.arguments.size(); position += 1) {
        if(auto error = builder.apply_cl(position)) {
            return std::unexpected(std::move(*error));
        }
    }
    builder.finish();
    return std::move(builder.command);
}

bool in_cl_mode(std::span<const Argument> arguments) {
    return std::ranges::any_of(arguments, [](const Argument& argument) {
        return argument.id == ID_driver_mode && lowercase(argument.values[0]) == "cl";
    });
}

}  // namespace

std::expected<ParsedCommand, Error> parse(const Identity& identity,
                                          std::span<const std::string> argv) {
    if(identity.dialect == Dialect::MSVC) {
        return parse_cl(argv);
    }
    auto arguments = read_arguments(argv, identity.dialect);
    if(!arguments.has_value()) {
        return std::unexpected(std::move(arguments.error()));
    }
    if(identity.dialect == Dialect::Clang && in_cl_mode(*arguments)) {
        return parse_cl(argv);
    }
    return parse_gnu(identity, std::move(*arguments));
}

std::string_view name(OutputKind kind) {
    switch(kind) {
        case OutputKind::PrimaryArtifact: return "primary-artifact";
        case OutputKind::ObjectFile: return "object-file";
        case OutputKind::LinkedArtifact: return "linked-artifact";
    }
    std::unreachable();
}

}  // namespace catter
