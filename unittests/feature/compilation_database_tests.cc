module;

#include "modules/prelude.h"

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
    auto root = kota::fs::TempDirectory::create(temp_dir, "catter-ut-");
    ZASSERT(root.has_value());
    std::string file(root->path());
    kota::fs::path::push(file, "compile_commands.json");
    ZASSERT(
        !kota::fs::write_file(file, R"([{"directory": "/", "file": "a.c", "arguments": ["cc"]}])"));
    ZASSERT(!kota::fs::set_permissions(file, 0200));

    bool thrown = false;
    try {
        feature::CompilationDatabase database("compile_commands.json",
                                              std::string(root->path()),
                                              true);
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
    auto root = kota::fs::TempDirectory::create(temp_dir, "catter-ut-");
    ZASSERT(root.has_value());
    std::string link(root->path());
    kota::fs::path::push(link, "compile_commands.json");
    std::string target(root->path());
    kota::fs::path::push(target, "build");
    ZASSERT(kota::fs::create_directory(target).has_value());
    kota::fs::path::push(target, "compile_commands.json");
    // The file the link names does not exist yet, as in a clean checkout.
    ZASSERT(!kota::fs::create_symlink("build/compile_commands.json", link));

    feature::CompilationDatabase database("compile_commands.json", std::string(root->path()), true);
    database.add(feature::JSON(kota::codec::dyn::Object{
        {"directory", root->path()                 },
        {"file",      "a.c"                        },
        {"arguments", kota::codec::dyn::Array{"cc"}},
    }));
    ZEXPECT(database.save() == target);
    auto linked = kota::fs::symlink_status(link);
    ZASSERT(linked.has_value());
    ZEXPECT(linked->type == kota::fs::FileType::Symlink);
    auto saved = kota::fs::status(target);
    ZASSERT(saved.has_value());
    ZEXPECT(saved->size > 0);
};

ZEST_CASE(a_loop_of_symlinks_is_no_database) {
    std::string temp_dir;
    ZASSERT(!kota::sys::temp_dir(temp_dir));
    auto root = kota::fs::TempDirectory::create(temp_dir, "catter-ut-");
    ZASSERT(root.has_value());
    std::string link(root->path());
    kota::fs::path::push(link, "compile_commands.json");
    ZASSERT(!kota::fs::create_symlink("compile_commands.json", link));

    bool thrown = false;
    try {
        feature::CompilationDatabase database("compile_commands.json",
                                              std::string(root->path()),
                                              false);
    } catch(const std::exception&) {
        thrown = true;
    }
    ZEXPECT(thrown);
    auto linked = kota::fs::symlink_status(link);
    ZASSERT(linked.has_value());
    ZEXPECT(linked->type == kota::fs::FileType::Symlink);
};
#endif

};  // ZEST_SUITE(builtin_compilation_database_tests)
