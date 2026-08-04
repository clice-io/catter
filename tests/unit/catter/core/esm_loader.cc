#include "js/esm_loader.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>
#include <kota/async/io/loop.h>

#include "js/js.h"
#include "js/qjs.h"

namespace fs = std::filesystem;
using namespace catter;

namespace {

template <typename Fn>
bool throws_with_message(Fn&& fn, std::string_view needle) {
    try {
        fn();
    } catch(const catter::qjs::Exception& error) {
        return std::string_view(error.what()).contains(needle);
    } catch(...) {}
    return false;
}

struct Fixture {
    Fixture() {
        static std::atomic_uint64_t serial{0};
        root = fs::temp_directory_path() /
               ("catter_esm_loader_" + std::to_string(serial.fetch_add(1)));
        fs::create_directories(root / "nested");
    }

    ~Fixture() {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    void write(const fs::path& path, std::string_view source) {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary);
        output << source;
    }

    fs::path root;
};

}  // namespace

TEST_SUITE(esm_loader_tests) {

TEST_CASE(path_resolution_uses_parent_without_extension_inference) {
    Fixture fixture;
    fixture.write(fixture.root / "nested" / "dep.js", "export const value = 42;");
    catter::js::EsmModuleLoader loader{};

    auto resolved =
        loader.normalizer((fixture.root / "main.js").string().c_str(), "./nested/dep.js");
    EXPECT_TRUE(fs::path(resolved) == fixture.root / "nested" / "dep.js");
    EXPECT_TRUE(throws_with_message(
        [&]() {
            (void)loader.normalizer((fixture.root / "main.js").string().c_str(), "./nested/dep");
        },
        "Cannot find module"));
}

TEST_CASE(path_resolution_rejects_directories_and_non_path_specifiers) {
    Fixture fixture;
    catter::js::EsmModuleLoader loader{};

    EXPECT_TRUE(throws_with_message(
        [&]() { (void)loader.normalizer((fixture.root / "main.js").string().c_str(), "./nested"); },
        "Directory import"));
    EXPECT_TRUE(throws_with_message(
        [&]() { (void)loader.normalizer((fixture.root / "main.js").string().c_str(), "package"); },
        "only file paths are supported"));
}

TEST_CASE(loader_reads_canonical_file_name) {
    Fixture fixture;
    fixture.write(fixture.root / "dep.js", "export const value = 42;");
    catter::js::EsmModuleLoader loader{};

    auto rt = qjs::Runtime::create();
    auto ctx = rt.context();

    auto module = loader.loader(ctx, (fixture.root / "dep.js").string().c_str());
    EXPECT_TRUE(module.module_def() != nullptr);
    EXPECT_TRUE(module.module_name().to_string() == (fixture.root / "dep.js").string());
}

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

TEST_CASE(loader_rejects_unknown_builtin_specifiers) {
    catter::js::EsmModuleLoader loader{};
    auto rt = qjs::Runtime::create();
    auto ctx = rt.context();

    EXPECT_TRUE(throws_with_message([&]() { (void)loader.loader(ctx, "catter/does-not-exist"); },
                                    "Unknown builtin module"));
}

struct ScriptRunConfig {
    std::string script_content;
    std::string script_path;
    fs::path working_directory;
};

kota::task<> async_run(ScriptRunConfig config) {
    catter::js::RuntimeScope runtime;

    std::exception_ptr error;
    try {
        runtime.start({.pwd = std::move(config.working_directory)});
        co_await catter::js::run_script(config.script_content, config.script_path);
    } catch(...) {
        error = std::current_exception();
    }

    co_await runtime.stop();

    if(error) {
        std::rethrow_exception(error);
    }
    co_return;
}

TEST_CASE(api_source_style_project_dependency_graph_is_resolved_once) {
    auto f = [&]() {
        Fixture fixture;
        fixture.write(fixture.root / "index.js",
                      "import { cdbValue } from './cdb/index.js';\n"
                      "import { optionValue } from './option/index.js';\n"
                      "import { compilerValue } from './cmd/compiler/index.js';\n"
                      "import { scriptValue } from './scripts/index.js';\n"
                      "import { sharedValue } from './data/flat-tree.js';\n"
                      "globalThis.projectResult = cdbValue + optionValue + compilerValue + "
                      "scriptValue + sharedValue;\n");

        // Mirrors api/src/data/index.ts and api/src/data/flat-tree.ts.
        fixture.write(fixture.root / "data" / "index.js",
                      "export { sharedValue } from './flat-tree.js';\n");
        fixture.write(fixture.root / "data" / "flat-tree.js",
                    "import { ioValue } from '../io.js';\n"
                    "globalThis.sharedLoads = (globalThis.sharedLoads || 0) + 1;\n"
                    "export const sharedValue = 40;\n");

        // Mirrors the root-level utility imports used throughout api/src.
        fixture.write(fixture.root / "index-helper.js", "export const rootHelper = 1;\n");
        fixture.write(fixture.root / "io.js",
                      "import './index-helper.js';\n" "export const ioValue = 41;\n");
        fixture.write(
            fixture.root / "fs.js",
            "import { ioValue } from './io.js';\n" "export const fsValue = ioValue + 1;\n");

        // Mirrors api/src/cdb/index.ts and cdb-manager.ts.
        fixture.write(fixture.root / "cdb" / "cdb.js",
                      "import { fsValue } from '../fs.js';\n" "export const cdbCore = fsValue;\n");
        fixture.write(
            fixture.root / "cdb" / "cdb-manager.js",
            "import { ioValue } from '../io.js';\n" "export const cdbManager = ioValue;\n");
        fixture.write(fixture.root / "cdb" / "index.js",
                    "import { cdbCore } from './cdb.js';\n"
                    "import { cdbManager } from './cdb-manager.js';\n"
                    "export const cdbValue = cdbCore + cdbManager;\n");

        // Mirrors api/src/option/index.ts importing both siblings and parent utilities.
        fixture.write(fixture.root / "option" / "types.js", "export const optionType = 1;\n");
        fixture.write(fixture.root / "option" / "index.js",
                    "import { optionType } from './types.js';\n"
                    "import { sharedValue } from '../data/flat-tree.js';\n"
                    "import { ioValue } from '../io.js';\n"
                    "export const optionValue = optionType + sharedValue + ioValue;\n");

        // Mirrors api/src/scripts/index.ts and its cdb/cmd-tree/view dependencies.
        fixture.write(
            fixture.root / "view" / "tree-renderer.js",
            "import { sharedValue } from '../data/index.js';\n" "export const viewValue = sharedValue;\n");
        fixture.write(fixture.root / "view" / "index.js",
                      "export { viewValue } from './tree-renderer.js';\n");
        fixture.write(fixture.root / "scripts" / "cdb.js",
                    "import { cdbValue } from '../cdb/index.js';\n"
                    "import { sharedValue } from '../data/index.js';\n"
                    "export const scriptCdb = cdbValue + sharedValue;\n");
        fixture.write(fixture.root / "scripts" / "cmd-tree.js",
                    "import { cdbValue } from '../cdb/index.js';\n"
                    "import { viewValue } from '../view/index.js';\n"
                    "export const scriptTree = cdbValue + viewValue;\n");
        fixture.write(fixture.root / "scripts" / "index.js",
                    "import { scriptCdb } from './cdb.js';\n"
                    "import { scriptTree } from './cmd-tree.js';\n"
                    "export const scriptValue = scriptCdb + scriptTree;\n");

        // Mirrors the deep api/src/cmd/compiler/{analysis,parsers,resolver} graph.
        fixture.write(
            fixture.root / "cmd" / "compiler" / "parsers" / "index.js",
            "import { optionValue } from '../../../option/index.js';\n" "export const parserValue = optionValue;\n");
        fixture.write(
            fixture.root / "cmd" / "compiler" / "resolver" / "index.js",
            "import { fsValue } from '../../../fs.js';\n" "export const resolverValue = fsValue;\n");
        fixture.write(fixture.root / "cmd" / "compiler" / "analysis.js",
                    "import { parserValue } from './parsers/index.js';\n"
                    "import { resolverValue } from './resolver/index.js';\n"
                    "export const analysisValue = parserValue + resolverValue;\n");
        fixture.write(
            fixture.root / "cmd" / "compiler" / "index.js",
            "import { analysisValue } from './analysis.js';\n" "export const compilerValue = analysisValue;\n");

        const auto entry_path = (fixture.root / "index.js").string();
        const auto bootstrap_path = (fixture.root / "bootstrap.js").string();

        constexpr auto script_content =  "import './index.js';\n"
                                    "if (globalThis.projectResult !== 575) {\n"
                                    "  throw new Error('unexpected project result');\n" "}\n"
                                    "if (globalThis.sharedLoads !== 1) {\n"
                                    "  throw new Error('shared module was evaluated more than once');\n"
                                    "}\n";

        auto task = async_run({.script_content = script_content,
                               .script_path = bootstrap_path,
                               .working_directory = fixture.root});
        kota::event_loop loop;
        loop.schedule(task);
        loop.run();
        task.result();

        catter::js::EsmModuleLoader path_loader{};
        const auto from_data_index =
            path_loader.normalizer((fixture.root / "data" / "index.js").string().c_str(),
                                   "./flat-tree.js");
        const auto from_option =
            path_loader.normalizer((fixture.root / "option" / "index.js").string().c_str(),
                                   "../data/flat-tree.js");
        const auto from_root = path_loader.normalizer(entry_path.c_str(), "./data/flat-tree.js");
        EXPECT_TRUE(from_data_index == from_option);
        EXPECT_TRUE(from_option == from_root);
    };

    EXPECT_NOTHROWS(f());
}

};  // TEST_SUITE(esm_loader_tests)
