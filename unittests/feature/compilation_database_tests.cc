module;

#include "modules/prelude.h"

#include "utils/temp_file_manager.h"

module catter;

import :feature.compilation_database;
import :feature.json;

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

ZEST_CASE(a_symlinked_database_stays_a_symlink) {
    std::string temp_dir;
    ZASSERT(!kota::sys::temp_dir(temp_dir));
    std::string root;
    ZASSERT(!kota::fs::create_temp_directory(root, temp_dir, "catter-ut-"));
    TempFileManager temp(root);
    ZASSERT(kota::fs::create_directory(temp.path("build")).has_value());
    // The file the link names does not exist yet, as in a clean checkout.
    ZASSERT(!kota::fs::create_symlink("build/compile_commands.json",
                                      temp.path("compile_commands.json")));

    feature::CompilationDatabase database("compile_commands.json", temp.root, true);
    database.add(feature::JSON(kota::codec::dyn::Object{
        {"directory", temp.root                    },
        {"file",      "a.c"                        },
        {"arguments", kota::codec::dyn::Array{"cc"}},
    }));
    ZEXPECT(database.save() == temp.path("build/compile_commands.json"));
    auto link = kota::fs::symlink_status(temp.path("compile_commands.json"));
    ZASSERT(link.has_value());
    ZEXPECT(link->type == kota::fs::FileType::Symlink);
    auto target = kota::fs::status(temp.path("build/compile_commands.json"));
    ZASSERT(target.has_value());
    ZEXPECT(target->size > 0);
};
#endif

};  // ZEST_SUITE(builtin_compilation_database_tests)
