#include "js/builtin_files.h"

#include <string_view>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

using namespace catter;

TEST_SUITE(builtin_files_tests) {

TEST_CASE(builtin_module_lookup_returns_embedded_sources) {
    const auto aggregate = catter::js::load_builtin_module("catter");
    EXPECT_TRUE(!aggregate.empty());
    // The aggregate entry re-exports the per-module bundles.
    EXPECT_TRUE(aggregate.find("catter/") != std::string_view::npos);

    const auto cdb = catter::js::load_builtin_module("catter/cdb");
    EXPECT_TRUE(!cdb.empty());
    EXPECT_TRUE(cdb.find("CDBManager") != std::string_view::npos);

    EXPECT_TRUE(catter::js::load_builtin_module("catter/does-not-exist").empty());
}

TEST_CASE(builtin_script_lookup_returns_embedded_sources) {
    const auto cdb = catter::js::load_builtin_script("script::cdb");
    EXPECT_TRUE(!cdb.empty());
    EXPECT_TRUE(cdb.find("catter/cdb") != std::string_view::npos);

    const auto cmd_tree = catter::js::load_builtin_script("script::cmd-tree");
    EXPECT_TRUE(!cmd_tree.empty());
    EXPECT_TRUE(cmd_tree.find("cmd-tree") != std::string_view::npos);

    const auto target_tree = catter::js::load_builtin_script("script::target-tree");
    EXPECT_TRUE(!target_tree.empty());
    EXPECT_TRUE(target_tree.find("target-tree") != std::string_view::npos);

    EXPECT_TRUE(catter::js::load_builtin_script("script::does-not-exist").empty());
}

};  // TEST_SUITE(builtin_files_tests)
