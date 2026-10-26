module;

#include "modules/prelude.h"

module catter:protocol.protocol;

import :session.data;

/// The messages catter and a handler exchange (docs/en/design/handler-protocol.md):
/// JSON-RPC 2.0, each message framed by a Content-Length header. Field names
/// are in lower camel case, as kota::ipc::JSONCodec writes them; the SDKs'
/// types are generated from the schema of these.
namespace catter::protocol {

/// The protocol's version, which `initialize` carries.
constexpr int version = 1;

/// Bytes that are not valid UTF-8, in base64.
struct Base64 {
    std::string base64;
};

/// Bytes from the system, such as an argument, a path or an environment
/// entry: a string when they are valid UTF-8, else their base64.
using OSString = std::variant<std::string, Base64>;

OSString to_os_string(std::string bytes);

std::vector<OSString> to_os_string(std::vector<std::string> bytes);

/// The bytes, or what is wrong with the base64.
std::expected<std::string, std::string> to_bytes(OSString text);

std::expected<std::vector<std::string>, std::string> to_bytes(std::vector<OSString> texts);

/// The build a session runs.
struct Build {
    std::vector<OSString> command;
    OSString cwd;
};

/// Request `initialize`, catter's first message.
struct InitializeParams {
    int version;
    /// Tells the sessions apart that one handler over TCP serves.
    std::string session;
    /// How the commands are captured: `inject`, or `replay` of a recorded
    /// build.
    std::string mode;
    Build build;
};

/// The commands a handler is told of, by what catter makes of them.
struct Pattern {
    /// The kind of program they run: `compiler`, `archiver` or `tool`.
    std::string kind;
    /// The names of those programs, compilers' and tools': `gcc`, `lld-elf`. Any when absent.
    std::optional<std::vector<std::string>> names;
    /// The phases of compilers' commands: `compile`, `link`. Any when absent.
    std::optional<std::vector<std::string>> phases;
};

/// The notifications a handler takes.
struct Events {
    /// `command`, for a handler that does not decide.
    bool command;
    bool error;
    bool exit;
};

struct InitializeResult {
    /// Every command comes as a request, answered with an action.
    bool decides;
    Events events;
    /// Commands carry their environment.
    bool env;
    /// Commands carry what catter makes of them.
    bool analysis;
    /// The commands the handler is told of: those any pattern names. Every command when
    /// absent.
    std::optional<std::vector<Pattern>> filter;
};

/// One argument of a command, as the option table of its program reads it.
struct Argument {
    /// The option it is, after aliases, by its name with its prefix: `-I` for `/I` too;
    /// `<input>` for an input, `<unknown>` for an option the table does not know.
    std::string option;
    /// How the command writes the option, `-msmall-data-limit=` of what is `-G`, `/I`; an
    /// input or an option the table does not know whole.
    OSString spelling;
    /// Where it starts in the command's arguments, the program's name at 0.
    std::uint32_t index;
    /// How many arguments it takes: 2 of `-o a.o`.
    std::uint32_t count;
    /// Its values, those an alias stands for included.
    std::vector<OSString> values;
};

/// The target a compiler compiles for.
struct Target {
    std::optional<std::string> triple;
    std::optional<std::string> os;
    std::optional<std::string> object_format;
    std::optional<std::string> env;
    /// How it names the files it makes: `elf`, `macho`, `coff-gnu` or `coff-msvc`.
    std::string artifact_model;
    /// What tells it: `argument`, `executable-prefix`, `driver-default` or `host-fallback`.
    std::string source;
};

/// A file a compiler reads.
struct Read {
    OSString path;
    /// The argument that names it, a position in `arguments`.
    std::uint32_t argument;
    /// The element of the command's arguments it is in.
    std::uint32_t index;
    /// `source`, or `link`.
    std::string role;
    /// The language of a source: `c` or `c++`.
    std::optional<std::string> language;
};

/// A file a compiler writes.
struct Write {
    OSString path;
    /// The reads it is made from, positions in `reads`.
    std::vector<std::uint32_t> reads;
    /// Why it is written: `explicit`, `default-output` or `assembly-listing`.
    std::string origin;
};

/// How catter inferred the files of a command, where its arguments leave them open.
struct Diagnostic {
    std::string code;
    std::string message;
    std::optional<OSString> path;
    /// Where the command gives the path: its argument and element of the command's arguments.
    std::optional<std::uint32_t> argument;
    std::optional<std::uint32_t> index;
};

/// A command's arguments with the response files they name read in.
struct Expansion {
    /// The response files read, as named, those they name too, in the order they are read.
    std::vector<OSString> files;
    /// The arguments with the files read in, the program's name first.
    std::vector<OSString> args;
};

/// What catter makes of a command that runs a compiler.
struct CompilerAnalysis {
    /// `gcc`, `clang`, `clang-cl` or `msvc`.
    std::string compiler;
    /// The syntax its arguments are read in: `gcc`, `clang`, or `msvc` (clang's in cl mode
    /// too).
    std::string dialect;
    std::string phase;
    std::string artifact;
    Target target;
    /// Its arguments with its response files read in, which the indices of `arguments`, `reads`
    /// and `diagnostics` count in; absent when it reads none, the indices then counting in its
    /// own.
    std::optional<Expansion> expansion;
    std::vector<Argument> arguments;
    std::vector<Read> reads;
    std::vector<Write> writes;
    std::vector<Diagnostic> diagnostics;
};

/// What catter makes of a command that runs `ar`.
struct ArchiverAnalysis {
    /// `p`, `q`, `r` or `t`.
    std::string operation;
    std::string modifiers;
    bool thin;
    OSString archive;
    std::vector<OSString> members;
    std::vector<OSString> reads;
    std::vector<OSString> writes;
};

/// What catter makes of a command that runs a tool: a linker or a librarian, whose arguments
/// it reads by the tool's option table.
struct ToolAnalysis {
    /// The option table: `lld-elf`, `lld-coff`, `lld-macho`, `lld-mingw`, `lld-wasm`,
    /// `llvm-lib` or `llvm-dlltool`.
    std::string tool;
    std::vector<Argument> arguments;
};

/// A command of a program catter knows, whose arguments it cannot read.
struct UnparsedAnalysis {
    /// The kind of program: `compiler`, `archiver` or `tool`.
    std::string of;
    /// Its name: a compiler's or a tool's, `ar`.
    std::string name;
    std::string error;
    std::string message;
};

KOTATSU_ANNOTATION(analysis_annotation,
                   tag = "kind",
                   tag_names = {"compiler", "archiver", "tool", "unparsed"});

/// What catter makes of a command.
using Analysis = kota::meta::annotate<analysis_annotation>::type<
    std::variant<CompilerAnalysis, ArchiverAnalysis, ToolAnalysis, UnparsedAnalysis>>;

/// Request `command` to a handler that decides, else a notification.
struct Command {
    data::ipcid_t id;
    /// The command that started this one, absent for the build command.
    std::optional<data::ipcid_t> parent;
    OSString cwd;
    OSString executable;
    std::vector<OSString> args;
    std::optional<std::vector<OSString>> env;
    /// What catter makes of it, to a handler that asks: none for a program catter knows
    /// nothing of.
    std::optional<Analysis> analysis;
};

struct Skip {};

struct Drop {};

struct Abort {
    std::string reason;
};

/// Runs another command in the command's place: absent fields keep the
/// command's own.
struct Modify {
    std::vector<OSString> args;
    std::optional<OSString> executable;
    std::optional<OSString> cwd;
    std::optional<std::vector<OSString>> env;
};

/// One edit of a command's arguments: flags to remove, or to add as they are written.
struct CommandEdit {
    /// `remove`, `append` or `prepend`.
    std::string kind;
    std::vector<OSString> flags;
};

/// Runs the command with its arguments edited, in turn.
struct Edit {
    std::vector<CommandEdit> edits;
};

KOTATSU_ANNOTATION(action_annotation,
                   tag = "action",
                   tag_names = {"skip", "drop", "abort", "modify", "edit"});

/// The result of `command`.
using Action =
    kota::meta::annotate<action_annotation>::type<std::variant<Skip, Drop, Abort, Modify, Edit>>;

/// Notification `error`: catter could not capture a command.
struct CaptureError {
    data::ipcid_t id;
    std::optional<data::ipcid_t> parent;
    std::string message;
};

/// Notification `exit`: a command has run.
struct Exit {
    data::ipcid_t id;
    std::int64_t code;
    /// From the decision on the command to its exit, in microseconds.
    std::int64_t duration;
};

/// Request `finish`, catter's last message: how the build ended.
struct Finish {
    std::int64_t code;
};

/// A program to run with its arguments, the program's name first.
struct Program {
    OSString executable;
    std::vector<OSString> args;
    /// The directory it runs in, which its relative response files are read from; without it,
    /// only its absolute ones are read.
    std::optional<OSString> cwd;
};

/// Request `analyze`, from a handler: what catter makes of programs the handler names.
struct AnalyzeParams {
    std::vector<Program> programs;
};

struct AnalyzeResult {
    /// One for each program, none for a program catter knows nothing of.
    std::vector<std::optional<Analysis>> analyses;
};

}  // namespace catter::protocol
