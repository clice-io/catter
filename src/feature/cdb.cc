#include <algorithm>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <cpptrace/exceptions.hpp>
#include <kota/deco/deco.h>
#include <kota/codec/dyn/dyn.h>

#include "command/compiler.h"
#include "feature/cli.h"
#include "feature/compilation_database.h"
#include "feature/descendants.h"
#include "feature/flat_tree.h"
#include "feature/handlers.h"
#include "feature/json.h"
#include "feature/path.h"
#include "feature/text.h"
#include "session/data.h"
#include "session/handler.h"
#include "support/encoding.h"

namespace catter::builtin {

namespace {

using kota::codec::dyn::Array;
using kota::codec::dyn::Object;

struct Options {
    DECO_CFG_START(required = false)

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"-o", "--output", "--output="},
                 meta_var = "<path>",
                 help = "Write the compilation database to this path.")
    <std::string> output;

    DecoFlag(names = {"--append"}, help = "Merge with an existing database. This is the default.")
    append = false;

    DecoFlag(names = {"--replace"},
             help = "Ignore existing database entries and replace the output file.")
    replace = false;

    DecoFlag(names = {"--save-on-failure"},
             help = "Save collected entries even when the build exits with a non-zero code.")
    save_on_failure = false;

    DecoFlag(names = {"--abort-on-command-failure"},
             help = "Abort when any captured command exits with a non-zero code.")
    abort_on_command_failure = false;

    DecoFlag(names = {"--abort-on-capture-error"},
             help = "Abort when catter reports a command capture error.")
    abort_on_capture_error = false;

    DecoFlag(names = {"-q", "--quiet"}, help = "Suppress informational output.")
    quiet = false;

    DecoFlag(names = {"-v", "--verbose"},
             help = "Print detailed command analysis and generated entries.")
    verbose = false;

    DecoInput(meta_var = "<path>", help = "Legacy output path; prefer --output for scripts.")
    <std::string> path;

    DecoFlag(names = {"-h", "--help"}, help = "Print this help message.")
    help = false;

    DECO_CFG_END()
};

const Help help{
    .name = "cdb",
    .description = "Generate a compile_commands.json file from captured compiler commands.",
    .examples =
        {
                   {.command = "cdb -o build/compile_commands.json"},
                   {
                .command = "cdb --save-on-failure -o compile_commands.json",
                .description =
                    "Merge existing entries and still save partial results from a failed build.",
            }, },
};

/// `argument` as a command line shows it: as it is when plain, else as a JSON string.
std::string quote_argument(std::string_view argument) {
    const auto plain = !argument.empty() && std::ranges::all_of(argument, [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               std::string_view(R"(_@%+=:,./\-)").contains(c);
    });
    return plain ? std::string(argument) : quote(argument);
}

std::string command_line(const data::command& command) {
    if(command.args.empty()) {
        return quote_argument(command.executable);
    }
    return command.args | std::views::transform(quote_argument) | std::views::join_with(' ') |
           std::ranges::to<std::string>();
}

/// `label:` and the values a line each, or that none is.
std::string list(std::string_view label, std::span<const std::string> values) {
    if(values.empty()) {
        return std::format("  {}: none recognized", label);
    }
    std::string text = std::format("  {}:", label);
    for(const auto& value: values) {
        text += std::format("\n    {}", value);
    }
    return text;
}

/// The first three distinct diagnostics, those about outputs first and rejected inputs last.
std::vector<std::string> notes_of(const cmd::CompilerAnalysis& analysis) {
    const auto priority = [](const cmd::Diagnostic& diagnostic) {
        const auto code = name(diagnostic.code);
        if(code.contains("output") || code.contains("assembly")) {
            return 0;
        }
        return diagnostic.code == cmd::Diagnostic::Code::InputCandidateRejected ? 2 : 1;
    };
    auto diagnostics = analysis.diagnostics;
    std::ranges::stable_sort(diagnostics, {}, priority);
    std::vector<std::string> notes;
    for(const auto& diagnostic: diagnostics) {
        auto note = diagnostic.path.has_value()
                        ? std::format("{}: {}", *diagnostic.path, diagnostic.message)
                        : diagnostic.message;
        if(!std::ranges::contains(notes, note)) {
            notes.push_back(std::move(note));
        }
        if(notes.size() == 3) {
            break;
        }
    }
    return notes;
}

std::string success_log(data::ipcid_t id,
                        const data::command& command,
                        const cmd::CompilerAnalysis& analysis) {
    const auto sources = analysis.reads | std::views::filter([](const cmd::Read& read) {
                             return read.role == cmd::InputRole::Source;
                         }) |
                         std::views::transform(&cmd::Read::path) | std::ranges::to<std::vector>();
    std::vector<std::string> outputs;
    for(const auto& write: analysis.writes) {
        switch(write.origin) {
            case cmd::WriteOrigin::Explicit: outputs.push_back(write.path); break;
            case cmd::WriteOrigin::DefaultOutput:
                outputs.push_back(std::format("{} (inferred from compiler defaults)", write.path));
                break;
            case cmd::WriteOrigin::AssemblyListing:
                outputs.push_back(std::format("{} (inferred assembly listing)", write.path));
                break;
        }
    }
    auto text = std::format("Successfully analyzed command #{}\n  at {}\n    {}\n{}\n{}",
                            id,
                            command.cwd,
                            command_line(command),
                            list("Sources", sources),
                            list("Outputs", outputs));
    if(auto notes = notes_of(analysis); !notes.empty()) {
        text += "\n  Resolver notes:";
        for(const auto& note: notes) {
            text += std::format("\n    {}", note);
        }
    }
    return text;
}

std::string failure_log(data::ipcid_t id, const data::command& command, const cmd::Error& error) {
    return std::format("Failed to analyze command #{}\n  at {}\n    {}\n  {}:\n    {}",
                       id,
                       command.cwd,
                       command_line(command),
                       name(error.kind),
                       error.message);
}

/// Writes the compilation database of the compiler commands of a build: an entry per source
/// file a compile reads and object it writes, which, when a command feeds another, names the
/// output of the command that compiles the source, not the build's final target.
class CDB final : public core::Handler {
public:
    explicit CDB(std::vector<std::string> args) : args(std::move(args)) {}

    kota::task<std::optional<core::Build>> on_start(core::Build build) override {
        auto parsed = parse_options<Options>(help, args);
        if(!parsed.has_value()) {
            co_return std::nullopt;
        }
        if(*parsed->append && *parsed->replace) {
            throw cpptrace::runtime_error("cdb: --append and --replace cannot be used together");
        }
        options = std::move(*parsed);
        output_path = options.output.value_or(options.path.value_or("build/compile_commands.json"));
        cwd = build.cwd;
        verbose_log(std::format(
            "CDB verbose logging enabled.\n  Output file: {}\n  Existing entries: {}\n  Resolver diagnostics: enabled",
            output_path,
            *options.replace ? "replace" : "merge"));
        co_return build;
    }

    kota::task<core::Action> on_command(data::ipcid_t id,
                                        std::optional<data::ipcid_t> parent,
                                        data::command command) override {
        descendants.add(id, parent);
        if(descendants.left_out(id)) {
            co_return core::Action{};
        }
        auto analysis = cmd::analyze_compiler(command.executable, command.args);
        if(!analysis.has_value()) {
            verbose_log(failure_log(id, command, analysis.error()));
            co_return core::Action{};
        }
        verbose_log(success_log(id, command, *analysis));
        compiler_commands.insert(id);

        for(const auto& read: analysis->reads) {
            if(read.role == cmd::InputRole::Source && read.path != "-") {
                sources[absolute_path(command.cwd, read.path)] = read.path;
            }
        }
        for(const auto& write: analysis->writes) {
            if(write.path == "-") {
                continue;
            }
            auto output = absolute_path(command.cwd, write.path);
            tree.merge(output, {});
            for(auto read: write.reads) {
                if(analysis->reads[read].path != "-") {
                    tree.merge(absolute_path(command.cwd, analysis->reads[read].path), {}, output);
                }
            }
            producers[output].push_back(Producer{.cwd = command.cwd, .argv = command.args});
        }
        descendants.leave_out_descendants_of(id);
        co_return core::Action{};
    }

    kota::task<> on_error(data::ipcid_t id,
                          [[maybe_unused]] std::optional<data::ipcid_t> parent,
                          std::string message) override {
        descendants.add(id, std::nullopt);
        if(*options.abort_on_capture_error) {
            throw cpptrace::runtime_error(std::move(message));
        }
        log(message);
        co_return;
    }

    kota::task<> on_exit(data::ipcid_t id, data::process_result result) override {
        if(descendants.left_out(id) || !*options.abort_on_command_failure || result.code == 0) {
            co_return;
        }
        if(*options.save_on_failure) {
            save();
        }
        throw cpptrace::runtime_error(
            std::format("CDB aborting after {}command {} exited with code {}.",
                        compiler_commands.contains(id) ? "compiler " : "",
                        id,
                        result.code));
    }

    kota::task<> on_finish(data::process_result result) override {
        if(result.code != 0 && !*options.save_on_failure) {
            log(std::format("Build failed with exit code {}. CDB will not be saved.", result.code));
            co_return;
        }
        save();
        co_return;
    }

private:
    /// A command that writes a file: where it runs and its argv, those of the file's entries.
    struct Producer {
        std::string cwd;

        std::vector<std::string> argv;
    };

    void log(std::string_view message) const {
        if(!*options.quiet) {
            std::println("{}", printable(message));
        }
    }

    void verbose_log(std::string_view message) const {
        if(*options.verbose) {
            log(message);
        }
    }

    /// The entries of the build: one per object a source is compiled to, and command that does,
    /// for each source the tree holds as a leaf.
    std::vector<JSON> entries() {
        tree.assemble();
        std::vector<JSON> entries;
        for(const auto& node: tree.nodes()) {
            if(!node.children.empty()) {
                continue;
            }
            auto source = sources.find(node.id);
            if(source == sources.end()) {
                continue;
            }
            for(const auto& output: node.parents) {
                auto made = producers.find(output);
                if(made == producers.end()) {
                    continue;
                }
                for(const auto& producer: made->second) {
                    Array arguments;
                    for(const auto& argument: producer.argv) {
                        arguments.push_back(JSON(std::string_view(argument)));
                    }
                    entries.push_back(Object{
                        {"directory", producer.cwd        },
                        {"file",      source->second      },
                        {"arguments", std::move(arguments)},
                        {"output",    output              },
                    });
                }
            }
        }
        return entries;
    }

    void save() {
        auto generated = entries();
        std::unordered_set<std::string_view> files;
        for(const auto& entry: generated) {
            files.insert(entry.as_object().find("file")->as_string());
        }
        verbose_log(std::format(
            "Generated {} entries for {} source files; {} entries include an output path.",
            generated.size(),
            files.size(),
            generated.size()));

        const auto inherit = !*options.replace;
        CompilationDatabase database(output_path, cwd, inherit);
        for(auto& entry: generated) {
            database.add(std::move(entry));
        }
        const auto& file = database.save();
        log(std::format("CDB saved to {} with {} entries.",
                        util::to_utf8(file),
                        database.entries().size()));
    }

private:
    std::vector<std::string> args;

    Options options;

    std::string output_path;

    /// The working directory, which a relative output path is from.
    std::string cwd;

    Descendants descendants;

    /// The files of the build: each output linked to the inputs it is made from.
    FlatTree<std::string, std::monostate> tree;

    /// The sources compiles read, as they name them, by their absolute paths.
    std::unordered_map<std::string, std::string> sources;

    /// The commands that write each output.
    std::unordered_map<std::string, std::vector<Producer>> producers;

    /// The commands analyzed as compilers.
    std::unordered_set<data::ipcid_t> compiler_commands;
};

}  // namespace

std::unique_ptr<core::Handler> make_cdb(std::vector<std::string> args) {
    return std::make_unique<CDB>(std::move(args));
}

}  // namespace catter::builtin
