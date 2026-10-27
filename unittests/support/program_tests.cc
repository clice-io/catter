module;

#include "modules/prelude.h"

#include "support/program.h"
#include "utils/temp_file_manager.h"

module catter;

using namespace catter;

namespace {

#ifdef CATTER_WINDOWS
constexpr std::string_view PROGRAM = "tool.exe";
#else
constexpr std::string_view PROGRAM = "tool";
#endif

/// `name` under `directory`.
std::string under(std::string_view directory, std::string_view name) {
    std::string path(directory);
    kota::fs::path::push(path, name);
    return path;
}

/// A directory of the test's own, under the current one, with `PROGRAM` in
/// `bin`.
struct Directory {
    TempFileManager files;

    explicit Directory(std::string_view name) : files(root(name)) {
        files.create(under("bin", PROGRAM));
    }

    static std::string root(std::string_view name) {
        std::string path;
        kota::fs::current_dir(path);
        return under(path, name);
    }
};

}  // namespace

ZEST_SUITE(program_tests) {

ZEST_CASE(path_is_taken_from_the_directory_given) {
    Directory directory("catter-program-path");
    const auto name = under("bin", PROGRAM);

    ZEXPECT(resolve_program(name, directory.files.root) == directory.files.path(name));
};

ZEST_CASE(path_found_nowhere_is_the_name_itself) {
    Directory directory("catter-program-missing");
    const auto name = under("bin", "catter-no-such-program");

    ZEXPECT(resolve_program(name, directory.files.root) == name);
};

#ifdef CATTER_WINDOWS
ZEST_CASE(name_is_looked_for_in_the_directory_given_as_the_current_one) {
    Directory directory("catter-program-name");

    ZEXPECT(resolve_program(PROGRAM, directory.files.path("bin")) ==
            directory.files.path(under("bin", PROGRAM)));
};
#endif

};  // ZEST_SUITE(program_tests)
