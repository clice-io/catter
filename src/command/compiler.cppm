module;

#include "modules/prelude.h"

export module catter.command:compiler;

import :error;
import :response_file;

export namespace catter {

/// The compilers catter recognizes by their executable's name.
enum class Compiler : std::uint8_t {
    GCC,
    Clang,
    ClangCL,
    MSVC,
    NVCC,
};

/// The syntax a compiler's arguments are parsed in.
enum class Dialect : std::uint8_t {
    Clang,
    GCC,
    MSVC,
    NVCC,
};

/// How far a compiler driver takes its inputs.
enum class Phase : std::uint8_t {
    Preprocess,
    SyntaxOnly,
    Compile,
    Link,
    Archive,
    /// `-r`: objects linked into one object.
    RelocatableLink,
};

/// What a compiler driver makes.
enum class Artifact : std::uint8_t {
    None,
    PreprocessedSource,
    /// What the sources include and import, written by `-M`, `-MM` and cl's
    /// `/scanDependencies`.
    Dependencies,
    Object,
    Executable,
    SharedLibrary,
    StaticLibrary,
    Assembly,
    LLVMIR,
    LLVMBitcode,
    PCH,
    PCM,
    /// Made by a mode of compiling catter does not model.
    Unknown,
};

enum class OS : std::uint8_t {
    Linux,
    Darwin,
    Windows,
};

enum class Environment : std::uint8_t {
    GNU,
    MinGW,
    MSVC,
};

enum class ObjectFormat : std::uint8_t {
    ELF,
    MachO,
    COFF,
};

/// The conventions a target names the files it makes by: `.o` or `.obj`, `.so`, `.dylib` or
/// `.dll`, ...
enum class ArtifactModel : std::uint8_t {
    ELF,
    MachO,
    COFFGNU,
    COFFMSVC,
};

/// Where the target of a command comes from.
enum class TargetSource : std::uint8_t {
    /// `--target=` or `-target`.
    Argument,
    /// The executable's name, `aarch64-linux-gnu-gcc`.
    ExecutablePrefix,
    /// The default target of MSVC's driver.
    DriverDefault,
    /// The platform catter runs on.
    HostFallback,
};

/// The target a command compiles for: what its triple says, and what files it names by.
struct Target {
    std::optional<std::string> triple;

    std::optional<OS> os;

    std::optional<ObjectFormat> object_format;

    std::optional<Environment> env;

    ArtifactModel model;

    TargetSource source;
};

/// The compiler an executable is, by its name.
struct Identity {
    Compiler compiler;

    /// The syntax its arguments are parsed in.
    Dialect dialect;

    /// Whether it is a C++ driver, `g++` or `clang++`, which reads C sources as C++.
    bool cxx = false;

    /// The target its name begins with, `aarch64-linux-gnu` of `aarch64-linux-gnu-gcc`; empty
    /// when it names none.
    std::string prefix;
};

/// One argument of a command as its compiler's option table reads it.
struct Argument {
    /// The option it is, after aliases: an id of the table (`option::clang::ID`), the table's id
    /// of an input or of an unknown option.
    std::uint32_t id;

    /// Where it starts in argv, its response files read in.
    std::uint32_t index;

    /// How many elements of argv it takes: 2 for `-o a.o`, 1 for `-Iinclude`.
    std::uint32_t count;

    /// How it is written: `-o` of `-o a.o`, `-I` of `-Iinclude`, an input as it is.
    std::string spelling;

    std::vector<std::string> values;

    /// The element of argv `values[i]` is in: the values after the argument's first element
    /// take one each, and those before are joined to the option, as in `-Iinclude`, `-Wl,a,b`
    /// or `/link/out:a.exe`.
    std::uint32_t value_index(std::size_t i) const {
        const auto joined = values.size() - (count - 1);
        return i < joined ? index : index + 1 + static_cast<std::uint32_t>(i - joined);
    }
};

/// Where a path is given: the argument it is in, a position in `CompilerAnalysis::arguments`,
/// and the element of argv it is in, its response files read in.
struct Location {
    std::uint32_t argument;

    std::uint32_t index;
};

/// The part of the compiler a file read is for.
enum class InputRole : std::uint8_t {
    /// Preprocessing and compiling: a source.
    Source,
    /// Linking or archiving.
    Link,
};

struct Read {
    std::string path;

    Location location;

    InputRole role;

    /// The language a source is read in, as `-x` names it: the one `/Tc` or `/Tp` gives it, the
    /// one `-x` before it sets, `/TC` or `/TP` for all, else its suffix's, `c`, `c++`,
    /// `c++-module`, `objective-c`, `assembler`, `cuda`, ... None for a file read for linking.
    std::optional<std::string> language;
};

/// Why a command writes a file.
enum class WriteOrigin : std::uint8_t {
    /// The command names it.
    Explicit,
    /// The compiler names it after an input, as it does without `-o`.
    DefaultOutput,
    /// MSVC's assembly listing, `/FA`, named after its source.
    AssemblyListing,
};

struct Write {
    std::string path;

    /// The reads it is made from, positions in `CompilerAnalysis::reads`.
    std::vector<std::uint32_t> reads;

    WriteOrigin origin;
};

/// How the files of a command were inferred, where its arguments leave it open.
struct Diagnostic {
    enum class Code : std::uint8_t {
        InputCandidateRejected,
        StreamInputIgnored,
        OutputKindIgnored,
        DefaultOutputMissingInput,
        DefaultOutputUnsupportedArtifact,
        DirectoryOutputUnsupportedArtifact,
        AssemblyListingAmbiguousOutput,
        /// A response file that cannot be read, whose arguments are unknown.
        ResponseFileUnread,
    };

    Code code;

    std::string message;

    /// The path it is about, when it is about one.
    std::optional<std::string> path;

    /// Where that path is given, when it is given in the command.
    std::optional<Location> location;
};

/// What catter makes of a command that runs a compiler driver.
struct CompilerAnalysis {
    Identity identity;

    /// The dialect its arguments are read in: the identity's, or MSVC's for clang in cl mode.
    Dialect dialect;

    /// Its argv with its response files read in, which its arguments are read from.
    Expansion expansion;

    /// Every argument after argv[0].
    std::vector<Argument> arguments;

    Phase phase;

    Artifact artifact;

    Target target;

    /// The files it reads, in the order the command names them.
    std::vector<Read> reads;

    /// The files it writes, each with the reads it is made from.
    std::vector<Write> writes;

    std::vector<Diagnostic> diagnostics;
};

/// The compiler `executable` is, by its name; none for a program that is no compiler catter
/// knows.
std::optional<Identity> identify(std::string_view executable);

/// Analyzes the command that runs `executable` with `argv`, argv[0] included, in `directory`,
/// whose response files `read` reads as the compiler would before it runs: cl's and clang's in
/// cl mode split as Windows splits a command line, the others as GCC does, but as clang's
/// `--rsp-quoting` says.
std::expected<CompilerAnalysis, Error> analyze_compiler(std::string_view executable,
                                                        std::span<const std::string> argv,
                                                        std::string_view directory,
                                                        ReadFile read);

/// The name of each value in catter's output: "gcc", "clang-cl", "preprocessed-source", ...
std::string_view name(Compiler compiler);
std::string_view name(Dialect dialect);
std::string_view name(Phase phase);
std::string_view name(Artifact artifact);
std::string_view name(OS os);
std::string_view name(Environment env);
std::string_view name(ObjectFormat format);
std::string_view name(ArtifactModel model);
std::string_view name(TargetSource source);
std::string_view name(Diagnostic::Code code);
std::string_view name(InputRole role);
std::string_view name(WriteOrigin origin);

}  // namespace catter
