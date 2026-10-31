module;

#include "support/prelude.h"

#include "utils/temp_file_manager.h"

module catter;

import :feature.compilation_database;
import :feature.json;

using namespace catter;

ZEST_SUITE(builtin_compilation_database_tests) {

ZEST_SUITE_ATTRS(serial = true);

#ifndef CATTER_WINDOWS
ZEST_CASE(unreadable_database_is_not_replaced) {
    TempFileManager temp("catter-ut-");
    ZASSERT(!temp.create("compile_commands.json",
                         R"([{"directory": "/", "file": "a.c", "arguments": ["cc"]}])"));
    const auto file = temp.path("compile_commands.json");
    ZASSERT(!fs::set_permissions(file, 0200));

    bool thrown = false;
    try {
        feature::CompilationDatabase database("compile_commands.json",
                                              std::string(temp.root.path()),
                                              true);
    } catch(const std::exception&) {
        thrown = true;
    }
    ZEXPECT(thrown);
    ZASSERT(!fs::set_permissions(file, 0600));
    auto status = fs::status(file);
    ZASSERT(status.has_value());
    ZEXPECT(status->size > 0);
};

ZEST_CASE(a_symlinked_database_stays_a_symlink) {
    TempFileManager temp("catter-ut-");
    ZASSERT(fs::create_directory(temp.path("build")).has_value());
    // The file the link names does not exist yet, as in a clean checkout.
    ZASSERT(!fs::create_symlink("build/compile_commands.json", temp.path("compile_commands.json")));

    feature::CompilationDatabase database("compile_commands.json",
                                          std::string(temp.root.path()),
                                          true);
    database.add(feature::JSON(codec::dyn::Object{
        {"directory", temp.root.path()       },
        {"file",      "a.c"                  },
        {"arguments", codec::dyn::Array{"cc"}},
    }));
    ZEXPECT(database.save() == temp.path("build/compile_commands.json"));
    auto link = fs::symlink_status(temp.path("compile_commands.json"));
    ZASSERT(link.has_value());
    ZEXPECT(link->type == fs::FileType::Symlink);
    auto target = fs::status(temp.path("build/compile_commands.json"));
    ZASSERT(target.has_value());
    ZEXPECT(target->size > 0);
};

ZEST_CASE(a_loop_of_symlinks_is_no_database) {
    TempFileManager temp("catter-ut-");
    const auto link = temp.path("compile_commands.json");
    ZASSERT(!fs::create_symlink("compile_commands.json", link));

    bool thrown = false;
    try {
        feature::CompilationDatabase database("compile_commands.json",
                                              std::string(temp.root.path()),
                                              false);
    } catch(const std::exception&) {
        thrown = true;
    }
    ZEXPECT(thrown);
    auto status = fs::symlink_status(link);
    ZASSERT(status.has_value());
    ZEXPECT(status->type == fs::FileType::Symlink);
};
#endif

};  // ZEST_SUITE(builtin_compilation_database_tests)
