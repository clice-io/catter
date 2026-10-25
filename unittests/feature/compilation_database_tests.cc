#include <filesystem>
#include <fstream>
#include <system_error>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "feature/compilation_database.h"
#include "support/encoding.h"
#include "utils/temp_file_manager.h"

using namespace catter;

ZEST_SUITE(builtin_compilation_database_tests) {

ZEST_SUITE_ATTRS(serial = true);

#ifndef CATTER_WINDOWS
ZEST_CASE(unreadable_database_is_not_replaced) {
    TempFileManager temp(fs::temp_directory_path() / "catter-ut-unreadable-cdb");
    std::error_code error;
    temp.create("compile_commands.json",
                error,
                R"([{"directory": "/", "file": "a.c", "arguments": ["cc"]}])");
    ASSERT(!error);
    const auto file = temp.root / "compile_commands.json";
    fs::permissions(file, fs::perms::owner_write, fs::perm_options::replace);

    bool thrown = false;
    try {
        builtin::CompilationDatabase database("compile_commands.json",
                                              util::to_utf8(temp.root),
                                              true);
    } catch(const std::exception&) {
        thrown = true;
    }
    EXPECT(thrown);
    fs::permissions(file,
                    fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace);
    EXPECT(fs::file_size(file) > 0);
};
#endif

};  // ZEST_SUITE(builtin_compilation_database_tests)
