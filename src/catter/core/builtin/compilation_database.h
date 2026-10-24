#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "builtin/json.h"

namespace catter::builtin {

/// A JSON compilation database, `compile_commands.json`: the entries already in its file, when
/// it inherits them, and those added, which replace the inherited entries of their source file.
/// Entries of a source file that are alike, in their output, command and arguments, are one.
class CompilationDatabase {
public:
    /// The database whose file the user names `path`, from the working directory `cwd`; with
    /// `inherit`, the entries of that file. Throws for a file that holds no database.
    CompilationDatabase(std::string_view path, std::string cwd, bool inherit);

    /// Adds an entry: `directory`, `file`, `arguments` and `output`.
    void add(JSON entry);

    /// The entries the database holds, those inherited first.
    std::vector<JSON> entries() const;

    /// Writes the database to its file, and returns the file.
    const std::filesystem::path& save() const;

private:
    /// Entries by the source file they compile, alike entries as one, in the order they came.
    struct Store {
        struct Group {
            std::vector<JSON> entries;

            std::unordered_map<std::string, std::size_t> positions;
        };

        void add(std::string_view cwd, JSON entry);

        std::vector<std::pair<std::string, Group>> groups;

        std::unordered_map<std::string, std::size_t> positions;
    };

private:
    std::filesystem::path file;

    std::string cwd;

    Store inherited;

    Store added;
};

}  // namespace catter::builtin
