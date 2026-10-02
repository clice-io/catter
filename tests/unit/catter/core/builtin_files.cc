#include "js/builtin_files.h"

#include <string_view>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

using namespace catter;

ZEST_SUITE(builtin_files_tests) {

ZEST_CASE(builtin_module_lookup_returns_embedded_sources) {
    // There is no aggregate "catter" module; every public module is a
    // subpath entry such as "catter/cdb" or "catter/cli".
    EXPECT(catter::js::load_builtin_module("catter").empty());

    const auto cdb = catter::js::load_builtin_module("catter/cdb");
    EXPECT(!cdb.empty());
    EXPECT(cdb.find("CDBManager") != std::string_view::npos);

    const auto cli = catter::js::load_builtin_module("catter/cli");
    EXPECT(!cli.empty());

    EXPECT(catter::js::load_builtin_module("catter/does-not-exist").empty());
}

ZEST_CASE(builtin_script_lookup_returns_embedded_sources) {
    const auto cdb = catter::js::load_builtin_script("script::cdb");
    EXPECT(!cdb.empty());
    EXPECT(cdb.find("catter/cdb") != std::string_view::npos);

    const auto cmd_tree = catter::js::load_builtin_script("script::cmd-tree");
    EXPECT(!cmd_tree.empty());
    EXPECT(cmd_tree.find("cmd-tree") != std::string_view::npos);

    const auto target_tree = catter::js::load_builtin_script("script::target-tree");
    EXPECT(!target_tree.empty());
    EXPECT(target_tree.find("target-tree") != std::string_view::npos);

    EXPECT(catter::js::load_builtin_script("script::does-not-exist").empty());
}

};  // ZEST_SUITE(builtin_files_tests)
