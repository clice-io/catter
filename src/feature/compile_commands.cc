module;

#include "modules/prelude.h"

module catter;

import catter.command;
import :feature.compilation_database;
import :feature.compile_commands;
import :feature.descendants;
import :feature.json;
import :feature.path;
import :feature.text;
import :session.data;
import :session.handler;

namespace catter::feature {

namespace {

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
std::vector<std::string> notes_of(const CompilerAnalysis& analysis) {
    const auto priority = [](const Diagnostic& diagnostic) {
        const auto code = name(diagnostic.code);
        if(code.contains("output") || code.contains("assembly")) {
            return 0;
        }
        return diagnostic.code == Diagnostic::Code::InputCandidateRejected ? 2 : 1;
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
                        const CompilerAnalysis& analysis) {
    const auto sources =
        analysis.reads |
        std::views::filter([](const Read& read) { return read.role == InputRole::Source; }) |
        std::views::transform(&Read::path) | std::ranges::to<std::vector>();
    std::vector<std::string> outputs;
    for(const auto& write: analysis.writes) {
        switch(write.origin) {
            case WriteOrigin::Explicit: outputs.push_back(write.path); break;
            case WriteOrigin::DefaultOutput:
                outputs.push_back(std::format("{} (inferred from compiler defaults)", write.path));
                break;
            case WriteOrigin::AssemblyListing:
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

std::string failure_log(data::ipcid_t id, const data::command& command, const Error& error) {
    return std::format("Failed to analyze command #{}\n  at {}\n    {}\n  {}:\n    {}",
                       id,
                       command.cwd,
                       command_line(command),
                       name(error.kind),
                       error.message);
}

/// Writes the compilation database of the compiler commands of a build: the entries of each
/// command, each of the source and the arguments of that command, but of a source another
/// compile writes, a preprocess's of a source another command compiles, and a probe's. A compile
/// into an object of what a preprocess wrote gets an entry of the preprocess's source, and so does
/// a preprocess whose file the build removes, as a build cache keyed by it does.
class CDB final : public Handler {
public:
    explicit CDB(CompileCommandsOptions options) : options(std::move(options)) {}

    kota::task<std::optional<Build>> on_start(Build build) override {
        output_path = options.output.value_or("build/compile_commands.json");
        // Where it fails, no file is in it.
        [[maybe_unused]] auto error = kota::sys::temp_dir(temporary);
        // A database catter cannot merge into ends the session before the build, not after.
        database.emplace(output_path, build.cwd, !*options.replace);
        verbose_log(std::format(
            "CDB verbose logging enabled.\n  Output file: {}\n  Existing entries: {}\n  Resolver diagnostics: enabled",
            output_path,
            *options.replace ? "replace" : "merge"));
        co_return build;
    }

    kota::task<Action> on_command(data::ipcid_t id,
                                  std::optional<data::ipcid_t> parent,
                                  data::command command) override {
        descendants.add(id, parent);
        if(descendants.left_out(id)) {
            co_return Action{};
        }
        // A compiler's children are steps of its own, nvcc's too, whose arguments catter does not
        // read yet.
        if(identify(command.executable).has_value()) {
            descendants.leave_out_descendants_of(id);
        }
        auto analysis =
            analyze_compiler(command.executable, command.args, command.cwd, kota::fs::read_file);
        if(!analysis.has_value()) {
            verbose_log(failure_log(id, command, analysis.error()));
            co_return Action{};
        }
        verbose_log(success_log(id, command, *analysis));
        compiler_commands.insert(id);

        const bool preprocess = analysis->phase == Phase::Preprocess;
        // What a preprocess writes from its one source to a file, as one with `-o` does.
        std::optional<Preprocessed> made;
        if(analysis->artifact == Artifact::PreprocessedSource && analysis->writes.size() == 1 &&
           analysis->writes.front().path != "-" && analysis->writes.front().reads.size() == 1) {
            const auto& write = analysis->writes.front();
            made = Preprocessed{
                .cwd = command.cwd,
                .analysis = *analysis,
                .read = write.reads.front(),
                .file = absolute_path(command.cwd, write.path),
            };
            preprocessed.insert_or_assign(made->file, *made);
            preprocesses.emplace(id, made->file);
        }
        for(auto& entry: entries_of(command.cwd, *analysis)) {
            const auto& object = entry.as_object();
            auto source = absolute_path(command.cwd, object.find("file")->as_string());
            const auto* output = object.find("output");
            auto from = preprocessed.find(source);
            if(from != preprocessed.end() && analysis->artifact == Artifact::Object &&
               output != nullptr) {
                const auto& by = from->second;
                const auto& read = by.analysis.reads[by.read];
                entry = entry_of(by.cwd, by.analysis, read, std::string(output->as_string()));
                source = absolute_path(by.cwd, read.path);
            }
            if(xmake_probe(source)) {
                continue;
            }
            if(!preprocess) {
                compiled.insert(source);
            }
            const bool existed = kota::fs::is_file(source);
            found.push_back({.entry = std::move(entry),
                             .source = std::move(source),
                             .preprocess = preprocess,
                             .existed = existed,
                             .made = made});
        }
        for(const auto& write: analysis->writes) {
            if(write.path != "-") {
                written.insert(absolute_path(command.cwd, write.path));
            }
        }
        co_return Action{};
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
        if(auto preprocess = preprocesses.extract(id);
           !preprocess.empty() && kota::fs::is_file(preprocess.mapped())) {
            kept_files.insert(std::move(preprocess.mapped()));
        }
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
    /// A preprocess of the build's, run in `cwd`, that writes `file` from `read` of `analysis`.
    struct Preprocessed {
        std::string cwd;

        CompilerAnalysis analysis;

        std::uint32_t read;

        std::string file;
    };

    /// An entry of the build's, of the source it names, absolute.
    struct Found {
        JSON entry;

        std::string source;

        /// Whether a preprocess made it, which describes the source only when no other command
        /// compiles it.
        bool preprocess;

        /// Whether the source was there when the command came: one the build then removes was
        /// a probe's, as a configure script's is.
        bool existed;

        /// The preprocess that made it, when it writes a file from the source.
        std::optional<Preprocessed> made;
    };

    /// Whether `source` is one xmake compiles to probe the compiler, which it keeps in its own
    /// directory of the temporary directory, `.xmake<uid>`.
    bool xmake_probe(std::string_view source) const {
        auto rest = kota::fs::path::strip_prefix(source, temporary);
        return rest.has_value() && rest->starts_with(".xmake");
    }

    void log(std::string_view message) const {
        if(!*options.quiet) {
            std::println("{}", kota::replace_invalid_utf8(message));
        }
    }

    void verbose_log(std::string_view message) const {
        if(*options.verbose) {
            log(message);
        }
    }

    void save() {
        std::size_t generated = 0;
        std::size_t outputs = 0;
        std::unordered_set<std::string_view> files;
        for(auto& [entry, source, preprocess, existed, made]: found) {
            if(written.contains(source) || (preprocess && compiled.contains(source)) ||
               (existed && !kota::fs::is_file(source))) {
                continue;
            }
            // A build cache that looks objects up by the preprocessed source, xmake's, removes it
            // once it has the object, from the cache or from a compile.
            if(made.has_value() && kept_files.contains(made->file) &&
               !kota::fs::is_file(made->file)) {
                entry = entry_of(made->cwd,
                                 made->analysis,
                                 made->analysis.reads[made->read],
                                 std::nullopt);
            }
            const auto& object = entry.as_object();
            const auto file = object.find("file")->as_string();
            generated += 1;
            outputs += object.find("output") != nullptr ? 1 : 0;
            files.insert(file);
            database->add(entry);
        }
        verbose_log(std::format(
            "Generated {} entries for {} source files; {} entries include an output path.",
            generated,
            files.size(),
            outputs));
        const auto& file = database->save();
        log(std::format("CDB saved to {} with {} entries.", file, database->entries().size()));
    }

private:
    CompileCommandsOptions options;

    std::string output_path;

    /// The database the entries go to: those it holds when the build starts, and the build's.
    std::optional<CompilationDatabase> database;

    Descendants descendants;

    /// The entries of the build's commands, in the order the commands came.
    std::vector<Found> found;

    /// The sources the build's commands but preprocesses read, absolute.
    std::unordered_set<std::string> compiled;

    /// The build's preprocesses, by the file each writes, absolute.
    std::unordered_map<std::string, Preprocessed> preprocessed;

    /// The preprocesses that have not exited, and the file each writes.
    std::unordered_map<data::ipcid_t, std::string> preprocesses;

    /// The files preprocesses wrote, there when they exited: a replayed build writes none.
    std::unordered_set<std::string> kept_files;

    /// The directory of temporary files.
    std::string temporary;

    /// The files the build's compiles write, absolute.
    std::unordered_set<std::string> written;

    /// The commands analyzed as compilers.
    std::unordered_set<data::ipcid_t> compiler_commands;
};

}  // namespace

std::unique_ptr<Handler> make_compile_commands(CompileCommandsOptions options) {
    return std::make_unique<CDB>(std::move(options));
}

}  // namespace catter::feature
