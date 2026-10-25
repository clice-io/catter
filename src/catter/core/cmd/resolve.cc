#include "cmd/resolve.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cmd/lowercase.h"
#include "util/encoding.h"

namespace catter::cmd {

namespace {

/// The suffixes a target names its files by.
struct Convention {
    std::string_view object;

    std::string_view executable;

    std::string_view shared_library;

    std::string_view static_library;
};

Convention convention_of(ArtifactModel model) {
    switch(model) {
        case ArtifactModel::ELF:
            return {
                .object = ".o",
                .executable = "",
                .shared_library = ".so",
                .static_library = ".a",
            };
        case ArtifactModel::MachO:
            return {
                .object = ".o",
                .executable = "",
                .shared_library = ".dylib",
                .static_library = ".a",
            };
        case ArtifactModel::COFFGNU:
            return {
                .object = ".o",
                .executable = ".exe",
                .shared_library = ".dll",
                .static_library = ".a",
            };
        case ArtifactModel::COFFMSVC:
            return {
                .object = ".obj",
                .executable = ".exe",
                .shared_library = ".dll",
                .static_library = ".lib",
            };
    }
    std::unreachable();
}

std::optional<std::string_view> extension_of(Artifact artifact, const Convention& convention) {
    switch(artifact) {
        case Artifact::Object: return convention.object;
        case Artifact::Executable: return convention.executable;
        case Artifact::SharedLibrary: return convention.shared_library;
        case Artifact::StaticLibrary: return convention.static_library;
        case Artifact::Assembly: return ".s";
        case Artifact::LLVMIR: return ".ll";
        case Artifact::LLVMBitcode: return ".bc";
        case Artifact::PCH: return ".pch";
        case Artifact::PCM: return ".pcm";
        case Artifact::None:
        case Artifact::PreprocessedSource:
        case Artifact::Unknown: return std::nullopt;
    }
    std::unreachable();
}

Target host_target() {
#if defined(CATTER_WINDOWS)
    return {.os = OS::Windows,
            .object_format = ObjectFormat::COFF,
            .model = ArtifactModel::COFFGNU,
            .source = TargetSource::HostFallback};
#elif defined(CATTER_MAC)
    return {.os = OS::Darwin,
            .object_format = ObjectFormat::MachO,
            .model = ArtifactModel::MachO,
            .source = TargetSource::HostFallback};
#else
    return {.os = OS::Linux,
            .object_format = ObjectFormat::ELF,
            .model = ArtifactModel::ELF,
            .source = TargetSource::HostFallback};
#endif
}

/// The target of a triple, alone: what it says, and the conventions of its files.
Target target_of(std::string_view triple, TargetSource source) {
    const auto lower = lowercase(triple);
    std::vector<std::string_view> tokens;
    for(std::string_view rest = lower; !rest.empty();) {
        const auto end = rest.find_first_of("-_");
        if(end != 0) {
            tokens.push_back(rest.substr(0, end));
        }
        if(end == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(end + 1);
    }
    const auto has = [&](std::string_view value) {
        return std::ranges::contains(tokens, value);
    };
    const auto has_any = [&](std::initializer_list<std::string_view> values) {
        return std::ranges::any_of(values, has);
    };

    Target target{.triple = std::string(triple), .source = source};
    if(has_any({"windows", "win32", "mingw32", "mingw64", "cygwin", "msys"})) {
        target.os = OS::Windows;
        target.object_format = ObjectFormat::COFF;
    } else if(has_any({"darwin", "macos", "ios", "apple"})) {
        target.os = OS::Darwin;
        target.object_format = ObjectFormat::MachO;
    } else if(has("linux")) {
        target.os = OS::Linux;
        target.object_format = ObjectFormat::ELF;
    }

    if(has("msvc")) {
        target.env = Environment::MSVC;
    } else if(std::ranges::any_of(tokens,
                                  [](std::string_view token) { return token.contains("mingw"); })) {
        target.env = Environment::MinGW;
    } else if(std::ranges::any_of(tokens, [](std::string_view token) {
                  return token.starts_with("gnu") || token == "musl" || token == "eabi" ||
                         token == "eabihf";
              })) {
        target.env = Environment::GNU;
    }

    if(!target.os.has_value()) {
        // A triple that names no OS is a bare-metal or a wasm target, whose files are named as
        // ELF's are.
        target.model = ArtifactModel::ELF;
    } else if(target.os == OS::Darwin) {
        target.model = ArtifactModel::MachO;
    } else if(target.os == OS::Linux) {
        target.model = ArtifactModel::ELF;
    } else if(target.env == Environment::GNU || target.env == Environment::MinGW ||
              has_any({"cygwin", "msys"})) {
        target.model = ArtifactModel::COFFGNU;
    } else {
        // Windows with no environment is MSVC's, as LLVM takes it.
        target.model = ArtifactModel::COFFMSVC;
    }
    return target;
}

Target select_target(const Identity& identity, const ParsedCommand& parsed) {
    if(parsed.triple.has_value()) {
        return target_of(*parsed.triple, TargetSource::Argument);
    }
    if(!identity.prefix.empty()) {
        return target_of(identity.prefix, TargetSource::ExecutablePrefix);
    }
    if(parsed.dialect == Dialect::MSVC) {
        return target_of("unknown-pc-windows-msvc", TargetSource::DriverDefault);
    }
    return host_target();
}

constexpr std::array<std::string_view, 2> c_suffixes = {".c", ".i"};
constexpr std::array<std::string_view, 6> cxx_suffixes =
    {".c++", ".cc", ".cp", ".cpp", ".cxx", ".ii"};

bool is_stream(std::string_view path) {
    return path == "-";
}

bool is_directory_like(std::string_view path) {
    return path.ends_with('/') || path.ends_with('\\');
}

std::string stem_of(std::string_view path) {
    return util::to_utf8(util::from_utf8(path).stem());
}

std::string join(std::string_view directory, std::string_view name) {
    return util::to_utf8((util::from_utf8(directory) / util::from_utf8(name)).lexically_normal());
}

/// The role of the file a positional argument names, or why it names none.
std::expected<InputRole, std::string_view> role_of(const Candidate& candidate,
                                                   const Convention& convention) {
    if(is_stream(candidate.path)) {
        return std::unexpected("stream input is not a filesystem dependency");
    }
    const auto path = lowercase(candidate.path);
    const auto ends_with = [&](std::string_view suffix) {
        return path.ends_with(suffix);
    };
    const auto language = lowercase(candidate.language.value_or("none"));
    if(language == "c") {
        if(std::ranges::any_of(c_suffixes, ends_with)) {
            return InputRole::Source;
        }
    } else if(language == "c++") {
        if(std::ranges::any_of(cxx_suffixes, ends_with)) {
            return InputRole::Source;
        }
    } else if(language == "none") {
        if(std::ranges::any_of(c_suffixes, ends_with) ||
           std::ranges::any_of(cxx_suffixes, ends_with)) {
            return InputRole::Source;
        }
        if(ends_with(convention.object) || ends_with(convention.shared_library) ||
           ends_with(convention.static_library)) {
            return InputRole::Link;
        }
    } else {
        return std::unexpected("input candidate has unsupported explicit language");
    }
    return std::unexpected("input candidate did not match suffix rules");
}

/// The language of the source `candidate` names: the one it is given, else its suffix's.
std::optional<std::string> source_language(const Candidate& candidate) {
    auto language = lowercase(candidate.language.value_or("none"));
    if(language != "none") {
        return language;
    }
    const auto path = lowercase(candidate.path);
    const auto ends_with = [&](std::string_view suffix) {
        return path.ends_with(suffix);
    };
    return std::ranges::any_of(c_suffixes, ends_with) ? "c" : "c++";
}

/// Builds the analysis from a parsed command: its reads first, then the writes made from them.
struct Builder {
    const ParsedCommand& parsed;

    Convention convention;

    std::vector<Read> reads;

    std::vector<Write> writes;

    std::vector<Diagnostic> diagnostics;

    void diagnose(Diagnostic::Code code,
                  std::string message,
                  std::optional<std::string> path = std::nullopt,
                  std::optional<Location> location = std::nullopt) {
        diagnostics.push_back(Diagnostic{
            .code = code,
            .message = std::move(message),
            .path = std::move(path),
            .location = location,
        });
    }

    void resolve_reads() {
        for(const auto& input: parsed.inputs) {
            if(is_stream(input.path)) {
                diagnose(Diagnostic::Code::StreamInputIgnored,
                         "stream input is not a filesystem dependency",
                         input.path,
                         input.location);
                continue;
            }
            reads.push_back(input);
        }
        for(const auto& candidate: parsed.candidates) {
            auto role = role_of(candidate, convention);
            if(!role.has_value()) {
                diagnose(Diagnostic::Code::InputCandidateRejected,
                         std::string(role.error()),
                         candidate.path,
                         candidate.location);
                continue;
            }
            reads.push_back(Read{
                .path = candidate.path,
                .location = candidate.location,
                .role = *role,
                .language = *role == InputRole::Source ? source_language(candidate) : std::nullopt,
            });
        }
        std::ranges::stable_sort(reads, {}, [](const Read& read) { return read.location.index; });
    }

    std::vector<std::uint32_t> sources() const {
        return std::views::iota(std::uint32_t(0), static_cast<std::uint32_t>(reads.size())) |
               std::views::filter(
                   [&](std::uint32_t read) { return reads[read].role == InputRole::Source; }) |
               std::ranges::to<std::vector>();
    }

    /// The reads the primary output is made from: the sources of a compile, every read of a
    /// link.
    std::vector<std::uint32_t> primary_reads() const {
        if(parsed.phase == Phase::Compile || parsed.phase == Phase::Preprocess) {
            return sources();
        }
        return std::views::iota(std::uint32_t(0), static_cast<std::uint32_t>(reads.size())) |
               std::ranges::to<std::vector>();
    }

    bool takes(OutputKind kind) const {
        switch(parsed.phase) {
            case Phase::Preprocess: return kind == OutputKind::PrimaryArtifact;
            case Phase::SyntaxOnly: std::unreachable();
            case Phase::Compile: return kind != OutputKind::LinkedArtifact;
            case Phase::Link: return kind != OutputKind::ObjectFile;
            case Phase::Archive:
            case Phase::RelocatableLink: return true;
        }
        std::unreachable();
    }

    /// The last output the phase takes.
    const Output* explicit_output() {
        const Output* selected = nullptr;
        for(const auto& output: parsed.outputs) {
            if(takes(output.kind)) {
                selected = &output;
                continue;
            }
            diagnose(Diagnostic::Code::OutputKindIgnored,
                     std::format("output kind {} is not used for {}",
                                 name(output.kind),
                                 name(parsed.phase)),
                     output.path,
                     output.location);
        }
        return selected;
    }

    void write(std::string path, std::vector<std::uint32_t> from, WriteOrigin origin) {
        writes.push_back(
            Write{.path = std::move(path), .reads = std::move(from), .origin = origin});
    }

    /// One write per read, each named after its read by `suffix`, in `directory` when given.
    void write_each(std::span<const std::uint32_t> from,
                    std::string_view suffix,
                    std::optional<std::string_view> directory,
                    WriteOrigin origin) {
        for(auto read: from) {
            auto file = stem_of(reads[read].path) + std::string(suffix);
            write(directory.has_value() ? join(*directory, file) : std::move(file), {read}, origin);
        }
    }

    void resolve_explicit(const Output& output) {
        auto from = primary_reads();
        const auto naming = parsed.phase == Phase::Compile
                                ? std::span<const std::uint32_t>(from)
                                : std::span<const std::uint32_t>(from).first(from.empty() ? 0 : 1);

        // An output that is a directory, `/Fo out\`, holds a file per input named after it.
        std::vector<std::string> paths;
        if(is_directory_like(output.path) && !naming.empty()) {
            if(auto extension = extension_of(parsed.artifact, convention)) {
                for(auto read: naming) {
                    paths.push_back(
                        join(output.path, stem_of(reads[read].path) + std::string(*extension)));
                }
            } else {
                diagnose(
                    Diagnostic::Code::DirectoryOutputUnsupportedArtifact,
                    std::format("cannot expand directory output for {}", name(parsed.artifact)),
                    output.path);
            }
        }
        if(paths.empty()) {
            paths.push_back(output.path);
        }

        if(paths.size() == from.size()) {
            for(std::size_t i = 0; i < paths.size(); i += 1) {
                write(std::move(paths[i]), {from[i]}, WriteOrigin::Explicit);
            }
            return;
        }
        for(auto& path: paths) {
            write(std::move(path), from, WriteOrigin::Explicit);
        }
    }

    void resolve_default_compile() {
        auto from = sources();
        if(from.empty()) {
            diagnose(Diagnostic::Code::DefaultOutputMissingInput,
                     "compile output has no source input to name");
            return;
        }
        auto extension = extension_of(parsed.artifact, convention);
        if(!extension.has_value()) {
            diagnose(Diagnostic::Code::DefaultOutputUnsupportedArtifact,
                     std::format("cannot infer default output for {}", name(parsed.artifact)));
            return;
        }
        write_each(from, *extension, std::nullopt, WriteOrigin::DefaultOutput);
    }

    /// The one output of a link, which only MSVC's driver names after its first input.
    void resolve_default_link() {
        auto from = primary_reads();
        if(from.empty()) {
            diagnose(Diagnostic::Code::DefaultOutputMissingInput,
                     "single output has no input to name");
            return;
        }
        if(parsed.dialect != Dialect::MSVC || parsed.phase != Phase::Link) {
            diagnose(Diagnostic::Code::ImplicitOutputUnresolved,
                     std::format("cannot infer implicit output for {} {}/{}",
                                 name(parsed.dialect),
                                 name(parsed.phase),
                                 name(parsed.artifact)));
            return;
        }
        const auto suffix = parsed.artifact == Artifact::Executable ? convention.executable
                                                                    : convention.shared_library;
        auto path = stem_of(reads[from.front()].path) + std::string(suffix);
        write(std::move(path), std::move(from), WriteOrigin::DefaultOutput);
    }

    void resolve_primary() {
        if(parsed.artifact == Artifact::None) {
            return;
        }
        if(const auto* output = explicit_output()) {
            resolve_explicit(*output);
            return;
        }
        switch(parsed.phase) {
            case Phase::Preprocess: return;
            case Phase::SyntaxOnly: std::unreachable();
            case Phase::Compile: resolve_default_compile(); return;
            case Phase::Link:
            case Phase::Archive:
            case Phase::RelocatableLink: resolve_default_link(); return;
        }
    }

    void resolve_listing() {
        if(!parsed.listing || parsed.phase == Phase::Preprocess ||
           parsed.phase == Phase::SyntaxOnly) {
            return;
        }
        auto from = sources();
        if(!parsed.listing_path.has_value()) {
            write_each(from, ".asm", std::nullopt, WriteOrigin::AssemblyListing);
            return;
        }
        const auto& path = *parsed.listing_path;
        if(is_directory_like(path)) {
            write_each(from, ".asm", path, WriteOrigin::AssemblyListing);
        } else if(from.size() == 1) {
            write(path, std::move(from), WriteOrigin::Explicit);
        } else {
            diagnose(Diagnostic::Code::AssemblyListingAmbiguousOutput,
                     "single assembly listing path cannot name multiple source inputs",
                     path);
        }
    }
};

}  // namespace

CompilerAnalysis resolve(Identity identity, ParsedCommand parsed) {
    auto target = select_target(identity, parsed);
    Builder builder{.parsed = parsed, .convention = convention_of(target.model)};
    builder.resolve_reads();
    builder.resolve_primary();
    builder.resolve_listing();
    return CompilerAnalysis{
        .identity = std::move(identity),
        .syntax = parsed.dialect,
        .arguments = std::move(parsed.arguments),
        .phase = parsed.phase,
        .artifact = parsed.artifact,
        .target = std::move(target),
        .reads = std::move(builder.reads),
        .writes = std::move(builder.writes),
        .diagnostics = std::move(builder.diagnostics),
    };
}

}  // namespace catter::cmd
