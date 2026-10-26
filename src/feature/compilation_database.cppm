module;

#include "modules/prelude.h"

module catter:feature.compilation_database;

import catter.command;
import :feature.json;

namespace catter::feature {

/// The entries of the compiler command run in `cwd` that `analysis` is of: one for each file it
/// writes from a source, and one without an output for a source it writes nothing from.
std::vector<JSON> entries_of(std::string_view cwd, const CompilerAnalysis& analysis);

/// A JSON compilation database, `compile_commands.json`: the entries already in its file, when
/// it inherits them, and those added, which replace the inherited entries of their source file
/// and output. Entries of a source file and output that are alike, in their command and
/// arguments, are one.
class CompilationDatabase {
public:
    /// The database whose file the user names `path`, from the working directory `cwd`; with
    /// `inherit`, the entries of that file. Throws for a path that is no file, and with
    /// `inherit` for a file that cannot be read or holds no database.
    CompilationDatabase(std::string_view path, std::string cwd, bool inherit);

    /// Adds an entry: `directory`, `file`, `arguments` and perhaps `output`.
    void add(JSON entry);

    /// The entries the database holds, those inherited first.
    std::vector<JSON> entries() const;

    /// Writes the database to its file, which a reader sees whole, before or after, and returns
    /// the file.
    const std::string& save() const;

private:
    /// Entries by the source file they compile and their output, alike entries as one, in the
    /// order they came.
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
    std::string file;

    std::string cwd;

    Store inherited;

    Store added;
};

}  // namespace catter::feature
