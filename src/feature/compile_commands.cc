module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter;

import catter.command;
import :feature.compilation_database;
import :feature.compile_commands;
import :feature.descendants;
import :feature.json;
import :session.handler;

namespace catter::feature {

namespace {

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
/// command, each of a source and the arguments of that command without its other sources, but of a
/// source another compile writes, a preprocess's of a source another command compiles, and a
/// probe's. A compile into an object of what a preprocess wrote gets an entry of the preprocess's
/// source, and so does a preprocess whose file the build removes, as a build cache keyed by it
/// does. A compile in a directory of symlinks the build removes, as Bazel's sandboxes are, gets the
/// entry of the directory they mirror.
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
        // A backend that only observes the build hears of a command once it runs: a build that
        // removes its response files when it has run may have removed them by then.
        for(const auto& diagnostic: analysis->diagnostics) {
            if(diagnostic.code == Diagnostic::Code::ResponseFileUnread) {
                uncaptured(std::format(
                    "Command {} names the response file `{}`, which catter could not read: the database lacks the arguments it holds",
                    id,
                    *diagnostic.path));
            }
        }
        compiler_commands.insert(id);

        const bool preprocess = analysis->phase == Phase::Preprocess;
        // What a preprocess writes from its one source to a file, as one with `-o` does, but cl's,
        // which no build tool compiles in two steps.
        std::optional<Preprocessed> made;
        if(analysis->artifact == Artifact::PreprocessedSource &&
           analysis->dialect != Dialect::MSVC && analysis->writes.size() == 1 &&
           analysis->writes.front().path != "-" && analysis->writes.front().reads.size() == 1) {
            const auto& write = analysis->writes.front();
            made = Preprocessed{
                .id = id,
                .cwd = command.cwd,
                .analysis = *analysis,
                .read = write.reads.front(),
            };
            kota::fs::path::resolve(made->file, write.path, command.cwd);
            preprocessed.insert_or_assign(made->file, *made);
            preprocesses.emplace(id, made->file);
        }
        auto entries = entries_of(command.cwd, *analysis);
        // The directory the working directory mirrors, when the build staged the sources there.
        std::optional<std::string> mirror;
        for(std::size_t i = 0; i < entries.size(); i += 1) {
            auto& entry = entries[i];
            const auto& object = entry.as_object();
            std::string source;
            kota::fs::path::resolve(source, object.find("file")->as_string(), command.cwd);
            const auto* output = object.find("output");
            std::optional<Mirrored> mirrored;
            auto from = preprocessed.find(source);
            if(from != preprocessed.end() && analysis->artifact == Artifact::Object &&
               output != nullptr) {
                const auto& by = from->second;
                const auto& read = by.analysis.reads[by.read];
                entry = entry_of(by.cwd, by.analysis, read, std::string(output->as_string()));
                consumed.insert(by.id);
                kota::fs::path::resolve(source, read.path, by.cwd);
            } else {
                if(auto directory = mirror_of(command.cwd, source)) {
                    mirrored = Mirrored{.entry = std::move(entries_of(*directory, *analysis)[i])};
                    kota::fs::path::resolve(mirrored->source,
                                            object.find("file")->as_string(),
                                            *directory);
                    if(!mirror.has_value()) {
                        mirror = std::move(directory);
                    }
                }
                if(!preprocess) {
                    compiled.insert(source);
                    if(mirrored.has_value()) {
                        compiled.insert(mirrored->source);
                    }
                }
            }
            if(xmake_probe(source) || cmake_probe(command.cwd, source)) {
                continue;
            }
            const bool existed = kota::fs::is_file(source);
            found.push_back({.entry = std::move(entry),
                             .source = std::move(source),
                             .preprocess = preprocess,
                             .existed = existed,
                             .made = made,
                             .mirrored = std::move(mirrored)});
        }
        for(const auto& write: analysis->writes) {
            if(write.path != "-") {
                std::string file;
                kota::fs::path::resolve(file, write.path, command.cwd);
                written.insert(std::move(file));
                if(mirror.has_value()) {
                    std::string there;
                    kota::fs::path::resolve(there, write.path, *mirror);
                    written.insert(std::move(there));
                }
            }
        }
        co_return Action{};
    }

    kota::task<> on_error(data::ipcid_t id,
                          [[maybe_unused]] std::optional<data::ipcid_t> parent,
                          std::string message) override {
        descendants.add(id, std::nullopt);
        uncaptured(std::move(message));
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
    /// The preprocess `id` of the build's, run in `cwd`, that writes `file` from `read` of
    /// `analysis`.
    struct Preprocessed {
        data::ipcid_t id;

        std::string cwd;

        CompilerAnalysis analysis;

        std::uint32_t read;

        std::string file;
    };

    /// The entry of a command's source in the directory its working directory mirrors.
    struct Mirrored {
        JSON entry;

        /// The source there, absolute.
        std::string source;
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

        /// The entry in the directory the command's working directory mirrors, when the build
        /// staged its sources there.
        std::optional<Mirrored> mirrored;
    };

    /// The directory the working directory `cwd` of a command mirrors, when the source of the
    /// command there, `source`, is a symlink to the file of the same name under it: a build that
    /// stages a command's inputs as symlinks in a directory of their own, as Bazel's sandboxes
    /// do, mirrors the directory it keeps them in.
    static std::optional<std::string> mirror_of(std::string_view cwd, std::string_view source) {
        const auto name = kota::fs::path::strip_prefix(source, cwd);
        std::string target;
        if(!name.has_value() || kota::fs::read_symlink(target, source)) {
            return std::nullopt;
        }
        std::string resolved;
        kota::fs::path::resolve(resolved, target, kota::fs::path::parent(source));
        // On Windows an absolute target reads `\\?\` first, which absolute() drops. It fails
        // only an empty path.
        std::string file;
        [[maybe_unused]] auto error = kota::fs::absolute(file, resolved);
        assert(!error);
        std::string directory = file;
        for([[maybe_unused]] auto component: kota::fs::path::components(*name)) {
            kota::fs::path::pop(directory);
        }
        std::string named;
        kota::fs::path::resolve(named, *name, directory);
        if(named != file) {
            return std::nullopt;
        }
        return directory;
    }

    /// Whether the entry of `source` of a command run in `cwd` is one CMake probes the compiler
    /// with, as a build configured under catter shows: one it runs in its own directory of the
    /// build, CMakeFiles, where it identifies the compiler and tries what it compiles, or of a
    /// file of its own modules.
    static bool cmake_probe(std::string_view cwd, std::string_view source) {
        if(std::ranges::contains(kota::fs::path::components(cwd), "CMakeFiles")) {
            return true;
        }
        // CMake's own directory, `share/cmake-3.31` or `share/cmake`, holds its modules.
        std::array<std::string_view, 2> previous;
        for(auto component: kota::fs::path::components(source)) {
            const auto [share, cmake] = previous;
            if(component == "Modules" && share == "share" &&
               (cmake == "cmake" || (cmake.starts_with("cmake-") && cmake.size() > 6 &&
                                     std::isdigit(static_cast<unsigned char>(cmake[6]))))) {
                return true;
            }
            previous = {cmake, component};
        }
        return false;
    }

    /// Whether `source` is one xmake compiles to probe the compiler, which it keeps in its own
    /// directory of the temporary directory, `.xmake<uid>`.
    bool xmake_probe(std::string_view source) const {
        auto rest = kota::fs::path::strip_prefix(source, temporary);
        return rest.has_value() && rest->starts_with(".xmake");
    }

    /// Reports what catter could not capture of a command, which ends the session with
    /// --abort-on-capture-error.
    void uncaptured(std::string message) const {
        if(*options.abort_on_capture_error) {
            throw cpptrace::runtime_error(std::move(message));
        }
        log(message);
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
        for(auto& [entry, source, preprocess, existed, made, mirrored]: found) {
            // Gone with the directory the build staged it in, the source is still where its
            // symlink led; gone alone, it was a probe's.
            const bool staged = existed && !kota::fs::is_file(source);
            if(staged) {
                if(!mirrored.has_value() || !kota::fs::is_file(mirrored->source)) {
                    continue;
                }
                entry = std::move(mirrored->entry);
                source = std::move(mirrored->source);
            }
            if(written.contains(source)) {
                continue;
            }
            // A compile of the source stands for every preprocess of it; a compile of what a
            // preprocess wrote, for that one alone.
            if(preprocess &&
               (compiled.contains(source) || (made.has_value() && consumed.contains(made->id)))) {
                continue;
            }
            // A build cache that looks objects up by the preprocessed source, xmake's, removes it
            // once it has the object, from the cache or from a compile; a staged preprocess's
            // file went with the directory it was written in.
            if(!staged && made.has_value() && kept_files.contains(made->file) &&
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

    /// The sources the build's commands but preprocesses compile, absolute.
    std::unordered_set<std::string> compiled;

    /// The preprocesses whose file a compile of the build read.
    std::unordered_set<data::ipcid_t> consumed;

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
