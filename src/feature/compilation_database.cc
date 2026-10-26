module;

#include "modules/prelude.h"

module catter;

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

/// Throws unless `entry`, which `context` names, is an entry as the format has it: a non-empty
/// `directory` and `file`, a `command` or `arguments`, and perhaps an `output`.
void validate(const JSON& entry, std::string_view context) {
    const auto fail = [&](std::string_view reason) {
        throw cpptrace::runtime_error(std::format("{}: {}", context, reason));
    };
    const auto* object = entry.get_object();
    if(object == nullptr) {
        fail("expected object item");
    }
    for(auto field: {"directory", "file"}) {
        const auto* value = object->find(field);
        if(value == nullptr || !value->is_string() || value->as_string().empty()) {
            fail(std::format(R"("{}" must be a non-empty string)", field));
        }
    }
    const auto* command = object->find("command");
    if(command != nullptr && !command->is_string()) {
        fail(R"("command" must be a string)");
    }
    const auto* arguments = object->find("arguments");
    if(arguments != nullptr &&
       (!arguments->is_array() || !std::ranges::all_of(arguments->as_array(), &JSON::is_string))) {
        fail(R"("arguments" must be a string array)");
    }
    if(command == nullptr && arguments == nullptr) {
        fail(R"(expected "command" or "arguments")");
    }
    const auto* output = object->find("output");
    if(output != nullptr && !output->is_string()) {
        fail(R"("output" must be a string)");
    }
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

    auto parsed = kota::codec::json::from_string<JSON>(content);
    if(!parsed.has_value()) {
        throw cpptrace::runtime_error(
            std::format("CDB file is not valid JSON: {}: {}", path, parsed.error().message));
    }
    auto* array = parsed->get_array();
    if(array == nullptr) {
        throw cpptrace::runtime_error(std::format("CDB file must contain a JSON array: {}", path));
    }
    std::vector<JSON> entries;
    for(std::size_t index = 0; index < array->size(); index += 1) {
        validate((*array)[index], std::format("{}[{}]", path, index));
        entries.push_back(std::move((*array)[index]));
    }
    return entries;
}

}  // namespace

void CompilationDatabase::Store::add(std::string_view cwd, JSON entry) {
    const auto& object = entry.as_object();
    const auto file = absolute_path(resolve(cwd, object.find("directory")->as_string()),
                                    object.find("file")->as_string());
    const auto field = [&](std::string_view name, JSON absent) {
        const auto* value = object.find(name);
        return value == nullptr ? std::move(absent) : *value;
    };
    const auto key = to_json(dyn::Array{
        field("output", ""),
        field("command", ""),
        field("arguments", dyn::Array{}),
    });

    auto [group_position, new_group] = positions.try_emplace(file, groups.size());
    if(new_group) {
        groups.emplace_back(file, Group{});
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
    auto error = created.has_value() ? kota::fs::write_file(file, to_json(dyn::Array(entries())))
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
