#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "build_replay.h"
#include "temp_file_manager.h"

namespace fs = std::filesystem;
using namespace catter;
using catter::tests::js::BuildReplay;
using catter::tests::js::BuildReplayConfig;
using catter::tests::js::ReplayCommand;

namespace {

class StdoutCapture {
public:
    explicit StdoutCapture(const fs::path& path) {
#ifdef _WIN32
        saved_fd_ = _dup(_fileno(stdout));
#else
        saved_fd_ = dup(fileno(stdout));
#endif
        if(saved_fd_ < 0) {
            throw std::runtime_error("failed to duplicate stdout");
        }
        if(freopen(path.string().c_str(), "w", stdout) == nullptr) {
#ifdef _WIN32
            _close(saved_fd_);
#else
            close(saved_fd_);
#endif
            throw std::runtime_error("failed to redirect stdout to " + path.string());
        }
    }

    ~StdoutCapture() {
        fflush(stdout);
#ifdef _WIN32
        _dup2(saved_fd_, _fileno(stdout));
        _close(saved_fd_);
#else
        dup2(saved_fd_, fileno(stdout));
        close(saved_fd_);
#endif
        clearerr(stdout);
    }

    StdoutCapture(const StdoutCapture&) = delete;
    StdoutCapture& operator= (const StdoutCapture&) = delete;

private:
    int saved_fd_ = -1;
};

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

ReplayCommand command(uint32_t id,
                      std::string exe,
                      std::vector<std::string> argv,
                      const fs::path& cwd,
                      uint32_t parent = 0) {
    return {
        .id = id,
        .parent = parent,
        .cwd = cwd.string(),
        .exe = std::move(exe),
        .argv = std::move(argv),
    };
}

BuildReplayConfig cdb_config(const fs::path& root, std::vector<std::string> script_args) {
    return {
        .script = "script::cdb",
        .script_args = std::move(script_args),
        .build_system_command = {"make"},
        .working_directory = root,
    };
}

ReplayCommand compile_command(uint32_t id,
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
    std::ofstream output(path, std::ios::binary);
    output << "[\n"
           << "  {\n"
           << "    \"directory\": \"" << root.string() << "\",\n"
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

    BuildReplay replay;
    replay.run(cdb_config(root, {"--output", save_path.string(), "--quiet"}),
               {
                   command(1, "make", {"make"}, root),
                   compile_command(2, root, "src/main.cc", "obj/main.o", 1),
                   command(3, "clang++", {"clang++", "obj/main.o", "-o", "bin/app"}, root, 1),
               },
               {.code = 0});

    const auto content = read_file(save_path);
    EXPECT_EQ(count_occurrences(content, "\"file\":"), 1);
    EXPECT_TRUE(content.find("\"file\": \"src/main.cc\"") != std::string::npos);
    EXPECT_TRUE(content.find("main.o") != std::string::npos);
}

TEST_CASE(cdb_does_not_save_on_failure_by_default) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto save_path = root / "compile_commands.json";

    BuildReplay replay;
    replay.run(cdb_config(root, {"--output", save_path.string(), "--quiet"}),
               {compile_command(1, root, "src/main.cc", "obj/main.o")},
               {.code = 1});

    EXPECT_TRUE(!fs::exists(save_path));
}

TEST_CASE(cdb_saves_on_failure_with_save_on_failure) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto save_path = root / "compile_commands.json";

    BuildReplay replay;
    replay.run(cdb_config(root, {"--output", save_path.string(), "--save-on-failure", "--quiet"}),
               {compile_command(1, root, "src/main.cc", "obj/main.o")},
               {.code = 1});

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

    BuildReplay replay;
    replay.run(cdb_config(root, {"--output", append_path.string(), "--quiet"}),
               {compile_command(1, root, "src/main.cc", "obj/main.o")},
               {.code = 0});
    replay.run(cdb_config(root, {"--output", replace_path.string(), "--replace", "--quiet"}),
               {compile_command(2, root, "src/main.cc", "obj/main.o")},
               {.code = 0});

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

    BuildReplay replay;
    auto broken = compile_command(1, root, "src/broken.cc", "obj/broken.o");
    broken.execution = catter::js::ProcessResult{.code = 2};
    bool aborted = false;
    try {
        replay.run(cdb_config(root,
                              {"--output",
                               save_path.string(),
                               "--abort-on-command-failure",
                               "--save-on-failure",
                               "--quiet"}),
                   {broken},
                   {.code = 0});
    } catch(const std::exception& error) {
        aborted =
            std::string_view(error.what()).find("exited with code 2") != std::string_view::npos;
    }

    EXPECT_TRUE(aborted);
    const auto content = read_file(save_path);
    EXPECT_EQ(count_occurrences(content, "\"file\":"), 1);
    EXPECT_TRUE(content.find("src/broken.cc") != std::string_view::npos);
}

TEST_CASE(cmd_tree_renders_captured_commands) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto output_path = root / "stdout.txt";

    BuildReplay replay;
    {
        StdoutCapture capture(output_path);
        replay.run({.script = "script::cmd-tree", .working_directory = root},
                   {
                       command(1, "make", {"make"}, root),
                       command(2, "clang++", {"clang++", "main.cc", "-c"}, root, 1),
                       command(3, "ld", {"ld", "main.o", "-o", "app"}, root, 1),
                   },
                   {.code = 0});
    }

    const auto rendered = read_file(output_path);
    EXPECT_TRUE(rendered.find("└──") != std::string::npos);
    EXPECT_TRUE(rendered.find("make") != std::string::npos);
    EXPECT_TRUE(rendered.find("clang++ main.cc -c") != std::string::npos);
    EXPECT_TRUE(rendered.find("ld main.o -o app") != std::string::npos);
}

TEST_CASE(cmd_tree_reports_empty_build) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto output_path = root / "stdout.txt";

    BuildReplay replay;
    {
        StdoutCapture capture(output_path);
        replay.run({.script = "script::cmd-tree", .working_directory = root}, {}, {.code = 0});
    }

    const auto rendered = read_file(output_path);
    EXPECT_TRUE(rendered.find("No commands found.") != std::string::npos);
}

TEST_CASE(target_tree_renders_target_forest) {
    TempFileManager cleanup(make_root());
    const auto root = cleanup.root;
    const auto output_path = root / "stdout.txt";

    BuildReplay replay;
    {
        StdoutCapture capture(output_path);
        replay.run({.script = "script::target-tree", .working_directory = root},
                   {
                       compile_command(1, root, "src/main.cc", "obj/main.o"),
                       command(2, "clang++", {"clang++", "obj/main.o", "-o", "bin/app"}, root),
                   },
                   {.code = 0});
    }

    const auto rendered = read_file(output_path);
    EXPECT_TRUE(rendered.find("└──") != std::string::npos);
    EXPECT_TRUE(rendered.find("main.cc") != std::string::npos);
    EXPECT_TRUE(rendered.find("main.o") != std::string::npos);
    EXPECT_TRUE(rendered.find("app") != std::string::npos);
}

};  // TEST_SUITE(build_replay_tests)
