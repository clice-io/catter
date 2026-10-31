module;

#include "support/prelude.h"

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

/// A directory of the test's own, with `PROGRAM` in `bin`.
struct Directory {
    TempFileManager files;

    explicit Directory(std::string_view name) : files(name) {
        std::string program;
        fs::path::push(program, "bin", PROGRAM);
        files.create(program);
    }
};

}  // namespace

ZEST_SUITE(program_tests) {

ZEST_CASE(path_is_taken_from_the_directory_given) {
    Directory directory("catter-program-path");
    std::string name;
    fs::path::push(name, "bin", PROGRAM);

    ZEXPECT(resolve_program(name, directory.files.root.path()) == directory.files.path(name));
};

ZEST_CASE(path_found_nowhere_is_the_name_itself) {
    Directory directory("catter-program-missing");
    std::string name;
    fs::path::push(name, "bin", "catter-no-such-program");

    ZEXPECT(resolve_program(name, directory.files.root.path()) == name);
};

#ifdef CATTER_WINDOWS
ZEST_CASE(name_is_looked_for_in_the_directory_given_as_the_current_one) {
    Directory directory("catter-program-name");

    const auto bin = directory.files.path("bin");
    std::string program;
    fs::path::push(program, bin, PROGRAM);

    ZEXPECT(resolve_program(PROGRAM, bin) == program);
};
#endif

};  // ZEST_SUITE(program_tests)
