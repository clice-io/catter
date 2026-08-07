#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "replay.h"
#include "temp_file_manager.h"

namespace fs = std::filesystem;
using namespace catter;
using catter::core::ReplayConfig;
using catter::core::ReplayEvent;
using catter::core::ReplayFile;
using catter::core::ReplayRunner;

namespace {

fs::path make_root() {
    static std::atomic_uint64_t serial{0};
    const auto root =
        fs::temp_directory_path() / ("catter_build_replay_" + std::to_string(serial.fetch_add(1)));
    std::error_code ec;
    fs::create_directories(root, ec);
    if(ec) {
        throw std::runtime_error("failed to create replay test root: " + ec.message());
    }
    return root;
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if(!input) {
        throw std::runtime_error("failed to open " + path.string());
    }
    return std::string(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
}

size_t count_occurrences(std::string_view haystack, std::string_view needle) {
    size_t count = 0;
    size_t pos = 0;
    while((pos = haystack.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

ReplayEvent command(uint32_t id,
                    std::string exe,
                    std::vector<std::string> argv,
                    const fs::path& cwd,
                    uint32_t parent = 0) {
    return {
        .id = id,
        .parent = parent == 0 ? std::optional<int64_t>{} : parent,
        .cwd = cwd.string(),
        .exe = std::move(exe),
        .argv = std::move(argv),
    };
}

ReplayConfig cdb_config(const fs::path& root, std::vector<std::string> script_args) {
    return {
        .script = "script::cdb",
        .script_args = std::move(script_args),
        .build_system_command = {"make"},
        .working_directory = root,
    };
}

ReplayEvent compile_command(uint32_t id,
                            const fs::path& root,
                            std::string source,
                            std::string object,
                            uint32_t parent = 0) {
    return command(id,
                   "clang++",
                   {"clang++", "-c", std::move(source), "-o", std::move(object)},
                   root,
                   parent);
}

void write_existing_database(const fs::path& path, const fs::path& root) {
    // Windows paths use backslashes, which are invalid as raw JSON escapes.
    // Normalize to forward slashes so the fixture parses on every platform.
    auto directory = root.string();
    std::replace(directory.begin(), directory.end(), '\\', '/');
    std::ofstream output(path, std::ios::binary);
    output << "[\n"
           << "  {\n"
           << "    \"directory\": \"" << directory << "\",\n"
           << "    \"file\": \"src/inherited.cc\",\n"
           << "    \"arguments\": [\"clang++\", \"-c\", \"src/inherited.cc\"]\n"
           << "  }\n"
           << "]\n";
}

}  // namespace

TEST_SUITE(build_replay_tests) {

TEST_CASE(cdb_generates_compile_database) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto save_path = root / "compile_commands.json";

    ReplayRunner replay;
    replay.run(
        cdb_config(root,
                   {
                       "--output",
                       save_path.string(),
                       "--quiet"
    }),
        {
            .version = 1,
            .events =
                {
                    command(1, "make", {"make"}, root),
                    compile_command(2, root, "src/main.cc", "obj/main.o", 1),
                    command(3, "clang++", {"clang++", "obj/main.o", "-o", "bin/app"}, root, 1),
                },
            .finish = js::ProcessResult{.code = 0},
        });

    const auto content = read_file(save_path);
    EXPECT_EQ(count_occurrences(content, "\"file\":"), 1);
    EXPECT_TRUE(content.find("\"file\": \"src/main.cc\"") != std::string::npos);
    EXPECT_TRUE(content.find("main.o") != std::string::npos);
}

TEST_CASE(cdb_does_not_save_on_failure_by_default) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto save_path = root / "compile_commands.json";

    ReplayRunner replay;
    replay.run(cdb_config(root, {"--output", save_path.string(), "--quiet"}),
               {
                   .version = 1,
                   .events = {compile_command(1, root, "src/main.cc", "obj/main.o")},
                   .finish = js::ProcessResult{.code = 1},
               });

    EXPECT_TRUE(!fs::exists(save_path));
}

TEST_CASE(cdb_saves_on_failure_with_save_on_failure) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto save_path = root / "compile_commands.json";

    ReplayRunner replay;
    replay.run(cdb_config(root, {"--output", save_path.string(), "--save-on-failure", "--quiet"}),
               {
                   .version = 1,
                   .events = {compile_command(1, root, "src/main.cc", "obj/main.o")},
                   .finish = js::ProcessResult{.code = 1},
               });

    const auto content = read_file(save_path);
    EXPECT_EQ(count_occurrences(content, "\"file\":"), 1);
    EXPECT_TRUE(content.find("src/main.cc") != std::string::npos);
}

TEST_CASE(cdb_append_and_replace_existing_database) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto append_path = root / "append.json";
    const auto replace_path = root / "replace.json";
    write_existing_database(append_path, root);
    write_existing_database(replace_path, root);

    ReplayRunner replay;
    replay.run(cdb_config(root, {"--output", append_path.string(), "--quiet"}),
               {
                   .version = 1,
                   .events = {compile_command(1, root, "src/main.cc", "obj/main.o")},
                   .finish = js::ProcessResult{.code = 0},
               });
    replay.run(cdb_config(root, {"--output", replace_path.string(), "--replace", "--quiet"}),
               {
                   .version = 1,
                   .events = {compile_command(2, root, "src/main.cc", "obj/main.o")},
                   .finish = js::ProcessResult{.code = 0},
               });

    const auto appended = read_file(append_path);
    EXPECT_EQ(count_occurrences(appended, "\"file\":"), 2);
    EXPECT_TRUE(appended.find("src/inherited.cc") != std::string::npos);
    EXPECT_TRUE(appended.find("src/main.cc") != std::string::npos);

    const auto replaced = read_file(replace_path);
    EXPECT_EQ(count_occurrences(replaced, "\"file\":"), 1);
    EXPECT_TRUE(replaced.find("src/inherited.cc") == std::string::npos);
    EXPECT_TRUE(replaced.find("src/main.cc") != std::string::npos);
}

TEST_CASE(cdb_aborts_on_command_failure_and_saves_partial_database) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto save_path = root / "compile_commands.json";

    ReplayRunner replay;
    auto broken = compile_command(1, root, "src/broken.cc", "obj/broken.o");
    broken.execution = js::ProcessResult{.code = 2};
    bool aborted = false;
    try {
        replay.run(cdb_config(root,
                              {"--output",
                               save_path.string(),
                               "--abort-on-command-failure",
                               "--save-on-failure",
                               "--quiet"}),
                   {
                       .version = 1,
                       .events = {broken},
                       .finish = js::ProcessResult{.code = 0},
                   });
    } catch(const std::exception& error) {
        aborted =
            std::string_view(error.what()).find("exited with code 2") != std::string_view::npos;
    }

    EXPECT_TRUE(aborted);
    const auto content = read_file(save_path);
    EXPECT_EQ(count_occurrences(content, "\"file\":"), 1);
    EXPECT_TRUE(content.find("src/broken.cc") != std::string_view::npos);
}

};  // TEST_SUITE(build_replay_tests)
