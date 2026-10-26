module;

#include "modules/prelude.h"

module catter;

import catter.command;
import :feature.compilation_database;
import :feature.json;
import :feature.path;

namespace catter::feature {

namespace {

namespace dyn = kota::codec::dyn;

std::string resolve(std::string_view cwd, std::string_view path) {
    if(kota::fs::path::is_absolute(path)) {
        return std::string(path);
    }
    std::string resolved(cwd);
    kota::fs::path::push(resolved, path);
    kota::fs::path::normalize(resolved);
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
    std::string text;
    if(auto error = kota::fs::read_file(text, file)) {
        if(error == std::errc::no_such_file_or_directory) {
            return {};
        }
        throw cpptrace::runtime_error(
            std::format("Failed to read the CDB file: {}: {}", path, error.message()));
    }

    std::string_view content = text;
    // A byte order mark, which editors on Windows may begin the file with.
    if(content.starts_with("\xEF\xBB\xBF")) {
        content.remove_prefix(3);
    }
    if(content.find_first_not_of(" \t\n\r") == std::string_view::npos) {
        return {};
    }

    const auto fail = [](std::string_view reason) {
        return cpptrace::runtime_error(std::format(
            "{}\ncatter merges the build's entries into it: give --replace to write a new database over it.",
            reason));
    };
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

}  // namespace

std::vector<JSON> entries_of(std::string_view cwd, const CompilerAnalysis& analysis) {
    dyn::Array arguments;
    for(const auto& argument: analysis.expansion.argv) {
        arguments.push_back(JSON(std::string_view(argument)));
    }
    std::vector<JSON> entries;
    auto add = [&](const Read& source, std::optional<std::string> output) {
        dyn::Object entry{
            {"directory", cwd        },
            {"file",      source.path},
            {"arguments", arguments  },
        };
        if(output.has_value()) {
            entry.insert("output", std::move(*output));
        }
        entries.push_back(std::move(entry));
    };
    std::vector<bool> written(analysis.reads.size());
    for(const auto& write: analysis.writes) {
        if(write.path == "-") {
            continue;
        }
        for(auto read: write.reads) {
            if(analysis.reads[read].role == InputRole::Source) {
                add(analysis.reads[read], absolute_path(cwd, write.path));
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

void CompilationDatabase::Store::add(std::string_view cwd, JSON entry) {
    const auto& object = entry.as_object();
    const auto directory = resolve(cwd, object.find("directory")->as_string());
    const auto field = [&](std::string_view name, JSON absent) {
        const auto* value = object.find(name);
        return value == nullptr ? std::move(absent) : *value;
    };
    const auto* output = object.find("output");
    const auto source = to_json(dyn::Array{
        absolute_path(directory, object.find("file")->as_string()),
        output == nullptr ? "" : absolute_path(directory, output->as_string()),
    });
    const auto key = to_json(dyn::Array{
        field("command", ""),
        field("arguments", dyn::Array{}),
    });

    auto [group_position, new_group] = positions.try_emplace(source, groups.size());
    if(new_group) {
        groups.emplace_back(source, Group{});
    }
    auto& group = groups[group_position->second].second;
    auto [entry_position, new_entry] = group.positions.try_emplace(key, group.entries.size());
    if(new_entry) {
        group.entries.push_back(std::move(entry));
    } else {
        group.entries[entry_position->second] = std::move(entry);
    }
}

CompilationDatabase::CompilationDatabase(std::string_view path, std::string cwd, bool inherit) :
    file(resolve(cwd, path)), cwd(std::move(cwd)) {
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
    for(const auto& [source, group]: inherited.groups) {
        if(!added.positions.contains(source)) {
            std::ranges::copy(group.entries, std::back_inserter(entries));
        }
    }
    for(const auto& [source, group]: added.groups) {
        std::ranges::copy(group.entries, std::back_inserter(entries));
    }
    return entries;
}

const std::string& CompilationDatabase::save() const {
    auto created = kota::fs::create_directories(kota::fs::path::parent(file));
    auto error = created.has_value()
                     ? kota::fs::write_file_atomic(file, to_json(dyn::Array(entries())))
                     : created.error();
    if(error) {
        throw cpptrace::runtime_error(
            std::format("Failed to write the compilation database `{}`: {}",
                        file,
                        error.message()));
    }
    return file;
}

}  // namespace catter::feature
