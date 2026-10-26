module;

#include "modules/prelude.h"

#include "utils/temp_file_manager.h"

module catter;

import :feature.compilation_database;

using namespace catter;

ZEST_SUITE(builtin_compilation_database_tests) {

ZEST_SUITE_ATTRS(serial = true);

#ifndef CATTER_WINDOWS
ZEST_CASE(unreadable_database_is_not_replaced) {
    std::string temp_dir;
    ZASSERT(!kota::sys::temp_dir(temp_dir));
    std::string root;
    ZASSERT(!kota::fs::create_temp_directory(root, temp_dir, "catter-ut-"));
    TempFileManager temp(root);
    ZASSERT(!temp.create("compile_commands.json",
                         R"([{"directory": "/", "file": "a.c", "arguments": ["cc"]}])"));
    const auto file = temp.path("compile_commands.json");
    ZASSERT(!kota::fs::set_permissions(file, 0200));

    bool thrown = false;
    try {
        feature::CompilationDatabase database("compile_commands.json", temp.root, true);
    } catch(const std::exception&) {
        thrown = true;
    }
    ZEXPECT(thrown);
    ZASSERT(!kota::fs::set_permissions(file, 0600));
    auto status = kota::fs::status(file);
    ZASSERT(status.has_value());
    ZEXPECT(status->size > 0);
};
#endif

};  // ZEST_SUITE(builtin_compilation_database_tests)
