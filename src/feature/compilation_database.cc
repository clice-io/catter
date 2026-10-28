module;

#include "modules/prelude.h"

module catter;

import catter.command;
import :feature.compilation_database;
import :feature.json;

namespace catter::feature {

namespace {

namespace dyn = kota::codec::dyn;

/// `path` from `cwd`. An absolute path stays as it is spelled, `..` and all, which the system
/// resolves through the symlinks before it.
std::string resolve(std::string_view cwd, std::string_view path) {
    if(kota::fs::path::is_absolute(path)) {
        return std::string(path);
    }
    std::string resolved;
    kota::fs::path::resolve(resolved, path, cwd);
    return resolved;
}

/// What makes `entry` no entry as the format has it, which has a non-empty `directory` and
/// `file`, a `command` or `arguments`, and perhaps an `output`; none for an entry.
std::optional<std::string> invalid(const JSON& entry) {
    const auto* object = entry.get_object();
    if(object == nullptr) {
        return "expected object item";
    }
    for(std::string_view field: {"directory", "file"}) {
        const auto* value = object->find(field);
        if(value == nullptr || !value->is_string() || value->as_string().empty()) {
            return std::format(R"("{}" must be a non-empty string)", field);
        }
    }
    const auto* command = object->find("command");
    if(command != nullptr && !command->is_string()) {
        return R"("command" must be a string)";
    }
    const auto* arguments = object->find("arguments");
    if(arguments != nullptr &&
       (!arguments->is_array() || !std::ranges::all_of(arguments->as_array(), &JSON::is_string))) {
        return R"("arguments" must be a string array)";
    }
    if(command == nullptr && arguments == nullptr) {
        return R"(expected "command" or "arguments")";
    }
    const auto* output = object->find("output");
    if(output != nullptr && !output->is_string()) {
        return R"("output" must be a string)";
    }
    return std::nullopt;
}

/// The entries of the database file `file`, which the user names `path`.
std::vector<JSON> read(std::string_view path, std::string_view file) {
    const auto fail = [](std::string_view reason) {
        return cpptrace::runtime_error(std::format(
            "{}\ncatter merges the build's entries into it: give --replace to write a new database over it.",
            reason));
    };
    std::string text;
    if(auto error = kota::fs::read_file(text, file)) {
        if(error == std::errc::no_such_file_or_directory) {
            return {};
        }
        throw fail(std::format("Failed to read the CDB file: {}: {}", path, error.message()));
    }

    std::string_view content = text;
    // A byte order mark, which editors on Windows may begin the file with.
    if(content.starts_with("\xEF\xBB\xBF")) {
        content.remove_prefix(3);
    }
    if(content.find_first_not_of(" \t\n\r") == std::string_view::npos) {
        return {};
    }

    auto parsed = kota::codec::json::from_string<JSON>(content);
    if(!parsed.has_value()) {
        throw fail(std::format("CDB file is not valid JSON: {}: {}", path, parsed.error().message));
    }
    auto* array = parsed->get_array();
    if(array == nullptr) {
        throw fail(std::format("CDB file must contain a JSON array: {}", path));
    }
    std::vector<JSON> entries;
    for(std::size_t index = 0; index < array->size(); index += 1) {
        if(auto reason = invalid((*array)[index])) {
            throw fail(std::format("{}[{}]: {}", path, index, *reason));
        }
        entries.push_back(std::move((*array)[index]));
    }
    return entries;
}

JSON entry(std::string_view cwd,
           std::string_view file,
           std::span<const std::string> argv,
           std::optional<std::string> output) {
    dyn::Array arguments;
    for(const auto& argument: argv) {
        arguments.push_back(JSON(std::string_view(argument)));
    }
    dyn::Object entry{
        {"directory", cwd                 },
        {"file",      file                },
        {"arguments", std::move(arguments)},
    };
    if(output.has_value()) {
        entry.insert("output", std::move(*output));
    }
    return entry;
}

}  // namespace

std::vector<JSON> entries_of(std::string_view cwd, const CompilerAnalysis& analysis) {
    std::vector<JSON> entries;
    auto add = [&](const Read& source, std::optional<std::string> output) {
        const auto& argv = analysis.expansion.argv;
        std::vector<bool> removed(argv.size());
        const auto remove = [&](const Argument& argument) {
            std::ranges::fill_n(removed.begin() + argument.index, argument.count, true);
        };
        for(const auto& read: analysis.reads) {
            if(&read != &source && read.role == InputRole::Source) {
                remove(analysis.arguments[read.location.argument]);
            }
        }
        // A `-x` names the language of the inputs after it: one only the other sources follow
        // names none, which clang warns of.
        bool any_after = false;
        bool left_after = false;
        for(const auto& argument: analysis.arguments | std::views::reverse) {
            if(argument.id == option::clang::ID_INPUT) {
                any_after = true;
                left_after = left_after || !removed[argument.index];
            } else if(argument.id == option::clang::ID_x && any_after && !left_after) {
                remove(argument);
            }
        }
        const auto own = std::views::iota(0uz, argv.size()) |
                         std::views::filter([&](std::size_t i) { return !removed[i]; }) |
                         std::views::transform([&](std::size_t i) { return argv[i]; }) |
                         std::ranges::to<std::vector>();
        entries.push_back(entry(cwd, source.path, own, std::move(output)));
    };
    std::vector<bool> written(analysis.reads.size());
    for(const auto& write: analysis.writes) {
        if(write.path == "-" || analysis.phase == Phase::Preprocess) {
            continue;
        }
        std::string output;
        kota::fs::path::resolve(output, write.path, cwd);
        for(auto read: write.reads) {
            if(analysis.reads[read].role == InputRole::Source) {
                add(analysis.reads[read], output);
                written[read] = true;
            }
        }
    }
    for(std::size_t read = 0; read < analysis.reads.size(); read += 1) {
        if(analysis.reads[read].role == InputRole::Source && !written[read]) {
            add(analysis.reads[read], std::nullopt);
        }
    }
    return entries;
}

JSON entry_of(std::string_view cwd,
              const CompilerAnalysis& preprocess,
              const Read& source,
              std::optional<std::string> output) {
    std::vector<std::string> mode{"-c"};
    if(output.has_value()) {
        mode.insert(mode.end(), {"-o", *output});
    }
    const std::array<CommandEdit, 2> edits{
        // `-E` and the options xmake adds to it, then `-c` and `-o`, which come back after them.
        CommandEdit{.kind = CommandEdit::Kind::Remove,
                    .flags = {"-E", "-P", "-fdirectives-only", "-fpch-preprocess", "-c", "-o"}},
        CommandEdit{.kind = CommandEdit::Kind::Append, .flags = std::move(mode)               },
    };
    auto argv = apply_edits(preprocess.expansion.argv,
                            preprocess.arguments,
                            syntax_of(preprocess.identity.compiler, preprocess.dialect),
                            edits);
    // The edits are of options every syntax of a compiler reads.
    assert(argv.has_value());
    return entry(cwd, source.path, *argv, std::move(output));
}

void CompilationDatabase::Store::add(std::string_view cwd, JSON entry) {
    const auto& object = entry.as_object();
    const auto directory = resolve(cwd, object.find("directory")->as_string());
    // A file of a database another tool wrote may be spelled with other separators on Windows.
    const auto key_of = [&](std::string_view path) {
        std::string absolute;
        kota::fs::path::resolve(absolute, path, directory);
        kota::fs::path::to_slash(absolute);
        return absolute;
    };
    const auto field = [&](std::string_view name, JSON absent) {
        const auto* value = object.find(name);
        return value == nullptr ? std::move(absent) : *value;
    };
    const auto* output = object.find("output");
    Group group{
        .file = key_of(object.find("file")->as_string()),
        .output = output == nullptr ? "" : key_of(output->as_string()),
    };
    const auto key = to_json(dyn::Array{
        directory,
        field("command", ""),
        field("arguments", dyn::Array{}),
    });

    auto [group_position, new_group] =
        positions.try_emplace(to_json(dyn::Array{group.file, group.output}), groups.size());
    if(new_group) {
        files.insert(group.file);
        groups.push_back(std::move(group));
    }
    auto& stored = groups[group_position->second];
    auto [entry_position, new_entry] = stored.positions.try_emplace(key, stored.entries.size());
    if(new_entry) {
        stored.entries.push_back(std::move(entry));
    } else {
        stored.entries[entry_position->second] = std::move(entry);
    }
}

CompilationDatabase::CompilationDatabase(std::string_view path, std::string cwd, bool inherit) :
    file(resolve(cwd, path)), cwd(std::move(cwd)) {
    // The save renames a new file over the path it writes, which would replace a symlink: the
    // database goes to the file the links lead to, which need not exist yet.
    if(auto error = kota::fs::follow_symlinks(file, file)) {
        throw cpptrace::runtime_error(
            std::format("Failed to follow the symlinks of the CDB path: {}: {}",
                        path,
                        error.message()));
    }
    if(auto status = kota::fs::status(file);
       status.has_value() && status->type != kota::fs::FileType::Regular) {
        throw cpptrace::runtime_error(std::format("CDB path is not a file: {}", path));
    }
    if(inherit) {
        for(auto& entry: read(path, file)) {
            inherited.add(this->cwd, std::move(entry));
        }
    }
}

void CompilationDatabase::add(JSON entry) {
    added.add(cwd, std::move(entry));
}

std::vector<JSON> CompilationDatabase::entries() const {
    std::vector<JSON> entries;
    for(const auto& group: inherited.groups) {
        const bool replaced =
            group.output.empty()
                ? added.files.contains(group.file)
                : added.positions.contains(to_json(dyn::Array{group.file, group.output}));
        if(!replaced) {
            std::ranges::copy(group.entries, std::back_inserter(entries));
        }
    }
    for(const auto& group: added.groups) {
        std::ranges::copy(group.entries, std::back_inserter(entries));
    }
    return entries;
}

const std::string& CompilationDatabase::save() const {
    save_file(file, to_json(dyn::Array(entries())), "compilation database");
    return file;
}

}  // namespace catter::feature
