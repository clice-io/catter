module;

#include "modules/prelude.h"

module catter.command;

import :executable;
import :parse;
import :resolve;

namespace catter {

namespace {

/// A compiler's executable: a name, perhaps after a directory, a target prefix and before a
/// version, `.exe`.
struct Rule {
    Compiler compiler;

    Dialect dialect;

    bool cxx;

    std::regex executable;
};

/// The rule of `compiler`, whose executable is `name`, followed by what `suffix` matches.
Rule rule_of(Compiler compiler,
             Dialect dialect,
             bool cxx,
             std::string_view name,
             std::string_view suffix) {
    return {
        .compiler = compiler,
        .dialect = dialect,
        .cxx = cxx,
        .executable = executable_pattern(name, suffix),
    };
}

/// In match order. A rule whose name has a group captures the target prefix.
const auto& rules() {
    const static std::array rules = {
        rule_of(Compiler::GCC, Dialect::GCC, false, R"((?:([^\\/]+)-)?cc)", ""),
        rule_of(Compiler::GCC, Dialect::GCC, true, R"((?:([^\\/]+)-)?c\+\+)", ""),
        rule_of(Compiler::GCC, Dialect::GCC, false, R"((?:([^\\/]+)-)?gcc)", version_suffix),
        rule_of(Compiler::GCC, Dialect::GCC, true, R"((?:([^\\/]+)-)?g\+\+)", version_suffix),
        rule_of(Compiler::Clang, Dialect::Clang, false, R"((?:([^\\/]+)-)?clang)", version_suffix),
        rule_of(Compiler::Clang,
                Dialect::Clang,
                true,
                R"((?:([^\\/]+)-)?clang\+\+)",
                version_suffix),
        rule_of(Compiler::ClangCL,
                Dialect::MSVC,
                false,
                R"((?:([^\\/]+)-)?clang-cl)",
                version_suffix),
        rule_of(Compiler::MSVC, Dialect::MSVC, false, "cl", ""),
        rule_of(Compiler::NVCC, Dialect::NVCC, false, R"((?:[^\\/]+-)?nvcc)", version_suffix),
    };
    return rules;
}

/// How the compiler `identity` names splits the response files of `argv`, which it reads before
/// its other arguments: clang as `--rsp-quoting` says, else as Windows splits a command line in
/// cl mode, which `--driver-mode=cl` sets.
Quoting quoting_of(const Identity& identity, std::span<const std::string> argv) {
    if(identity.compiler != Compiler::Clang && identity.compiler != Compiler::ClangCL) {
        return identity.dialect == Dialect::MSVC ? Quoting::Windows : Quoting::GNU;
    }
    auto quoting =
        identity.dialect == Dialect::MSVC || std::ranges::contains(argv, "--driver-mode=cl")
            ? Quoting::Windows
            : Quoting::GNU;
    for(const auto& argument: argv) {
        if(argument == "--rsp-quoting=posix") {
            quoting = Quoting::GNU;
        } else if(argument == "--rsp-quoting=windows") {
            quoting = Quoting::Windows;
        }
    }
    return quoting;
}

}  // namespace

std::optional<Identity> identify(std::string_view executable) {
    for(const auto& rule: rules()) {
        std::match_results<std::string_view::const_iterator> match;
        if(std::regex_match(executable.begin(), executable.end(), match, rule.executable)) {
            return Identity{
                .compiler = rule.compiler,
                .dialect = rule.dialect,
                .cxx = rule.cxx,
                .prefix = match.size() > 1 ? match[1].str() : std::string(),
            };
        }
    }
    return std::nullopt;
}

std::expected<CompilerAnalysis, Error> analyze_compiler(std::string_view executable,
                                                        std::span<const std::string> argv,
                                                        std::string_view directory,
                                                        ReadFile read) {
    auto identity = identify(executable);
    if(!identity.has_value()) {
        return std::unexpected(Error{
            .kind = Error::Kind::CompilerUnsupported,
            .message = "unsupported compiler dialect: unknown",
        });
    }
    if(identity->dialect == Dialect::NVCC) {
        return std::unexpected(Error{
            .kind = Error::Kind::CompilerUnsupported,
            .message = "compiler dialect is not supported yet: nvcc",
        });
    }
    auto expansion = expand_response_files(argv, directory, quoting_of(*identity, argv), read);
    auto parsed = parse(*identity, expansion.argv);
    if(!parsed.has_value()) {
        return std::unexpected(std::move(parsed.error()));
    }
    return resolve(std::move(*identity), std::move(expansion), std::move(*parsed));
}

std::string_view name(Compiler compiler) {
    switch(compiler) {
        case Compiler::GCC: return "gcc";
        case Compiler::Clang: return "clang";
        case Compiler::ClangCL: return "clang-cl";
        case Compiler::MSVC: return "msvc";
        case Compiler::NVCC: return "nvcc";
    }
    std::unreachable();
}

std::string_view name(Dialect dialect) {
    switch(dialect) {
        case Dialect::Clang: return "clang";
        case Dialect::GCC: return "gcc";
        case Dialect::MSVC: return "msvc";
        case Dialect::NVCC: return "nvcc";
    }
    std::unreachable();
}

std::string_view name(Phase phase) {
    switch(phase) {
        case Phase::Preprocess: return "preprocess";
        case Phase::SyntaxOnly: return "syntax-only";
        case Phase::Compile: return "compile";
        case Phase::Link: return "link";
        case Phase::Archive: return "archive";
        case Phase::RelocatableLink: return "relocatable-link";
    }
    std::unreachable();
}

std::string_view name(Artifact artifact) {
    switch(artifact) {
        case Artifact::None: return "none";
        case Artifact::PreprocessedSource: return "preprocessed-source";
        case Artifact::Object: return "object";
        case Artifact::Executable: return "exe";
        case Artifact::SharedLibrary: return "shared";
        case Artifact::StaticLibrary: return "static-lib";
        case Artifact::Assembly: return "asm";
        case Artifact::LLVMIR: return "llvm-ir";
        case Artifact::LLVMBitcode: return "llvm-bc";
        case Artifact::PCH: return "pch";
        case Artifact::PCM: return "pcm";
        case Artifact::Unknown: return "unknown";
    }
    std::unreachable();
}

std::string_view name(OS os) {
    switch(os) {
        case OS::Linux: return "linux";
        case OS::Darwin: return "darwin";
        case OS::Windows: return "windows";
    }
    std::unreachable();
}

std::string_view name(Environment env) {
    switch(env) {
        case Environment::GNU: return "gnu";
        case Environment::MinGW: return "mingw";
        case Environment::MSVC: return "msvc";
    }
    std::unreachable();
}

std::string_view name(ObjectFormat format) {
    switch(format) {
        case ObjectFormat::ELF: return "elf";
        case ObjectFormat::MachO: return "macho";
        case ObjectFormat::COFF: return "coff";
    }
    std::unreachable();
}

std::string_view name(ArtifactModel model) {
    switch(model) {
        case ArtifactModel::ELF: return "elf";
        case ArtifactModel::MachO: return "macho";
        case ArtifactModel::COFFGNU: return "coff-gnu";
        case ArtifactModel::COFFMSVC: return "coff-msvc";
    }
    std::unreachable();
}

std::string_view name(TargetSource source) {
    switch(source) {
        case TargetSource::Argument: return "argument";
        case TargetSource::ExecutablePrefix: return "executable-prefix";
        case TargetSource::DriverDefault: return "driver-default";
        case TargetSource::HostFallback: return "host-fallback";
    }
    std::unreachable();
}

std::string_view name(Diagnostic::Code code) {
    switch(code) {
        case Diagnostic::Code::InputCandidateRejected: return "input-candidate-rejected";
        case Diagnostic::Code::StreamInputIgnored: return "stream-input-ignored";
        case Diagnostic::Code::OutputKindIgnored: return "output-kind-ignored";
        case Diagnostic::Code::DefaultOutputMissingInput: return "default-output-missing-input";
        case Diagnostic::Code::DefaultOutputUnsupportedArtifact:
            return "default-output-unsupported-artifact";
        case Diagnostic::Code::ImplicitOutputUnresolved: return "implicit-output-unresolved";
        case Diagnostic::Code::DirectoryOutputUnsupportedArtifact:
            return "directory-output-unsupported-artifact";
        case Diagnostic::Code::AssemblyListingAmbiguousOutput:
            return "assembly-listing-ambiguous-output";
        case Diagnostic::Code::ResponseFileUnread: return "response-file-unread";
    }
    std::unreachable();
}

std::string_view name(InputRole role) {
    switch(role) {
        case InputRole::Source: return "source";
        case InputRole::Link: return "link";
    }
    std::unreachable();
}

std::string_view name(WriteOrigin origin) {
    switch(origin) {
        case WriteOrigin::Explicit: return "explicit";
        case WriteOrigin::DefaultOutput: return "default-output";
        case WriteOrigin::AssemblyListing: return "assembly-listing";
    }
    std::unreachable();
}

}  // namespace catter
