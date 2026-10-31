#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <kotatsu/zest/zest.h>
#include <kotatsu/support/utf16.h>
#include <kotatsu/fs/directory.h>
#include <kotatsu/fs/file.h>
#include <kotatsu/fs/path.h>
#include <kotatsu/fs/resolve.h>
#include <kotatsu/sys/env.h>

#include "hook/windows/create_process.h"

namespace ct = catter::hook;

namespace {

/// A file `name` in directory `directory` under the current one, which goes
/// with it. Tests run at once: each takes a directory of its own.
struct ScopedFile {
    std::string path;

    ScopedFile(std::string_view directory, std::string_view name) {
        kotatsu::fs::current_dir(path);
        kotatsu::fs::path::push(path, directory);
        kotatsu::fs::create_directories(path);
        kotatsu::fs::path::push(path, name);
        kotatsu::fs::write_file(path, "");
    }

    ~ScopedFile() {
        kotatsu::fs::remove_all(kotatsu::fs::path::parent(path));
    }
};

std::wstring wide(std::string_view text) {
    std::wstring out;
    kotatsu::to_utf16(out, text);
    return out;
}

ZEST_SUITE(create_process) {
ZEST_CASE(application_name_is_taken_from_the_current_directory) {
    ScopedFile tool("catter-application-name", "tool.exe");

    ZEXPECT(ct::resolve_program(LR"(catter-application-name\tool.exe)", L"tool a b") == tool.path);
};

ZEST_CASE(command_line_path_is_taken_from_the_current_directory) {
    ScopedFile tool("catter-command-line-path", "tool.exe");

    ZEXPECT(ct::resolve_program(nullptr, LR"(.\catter-command-line-path\tool.exe a b)") ==
            tool.path);
};

ZEST_CASE(unquoted_path_with_blanks_is_read_up_to_each_blank_in_turn) {
    ScopedFile tool("unquoted path tests", "tool.exe");

    ZEXPECT(ct::resolve_program(nullptr, wide(tool.path + " a b").c_str()) == tool.path);
    ZEXPECT(ct::resolve_program(nullptr, wide(tool.path).c_str()) == tool.path);
    ZEXPECT(ct::resolve_program(nullptr, wide(std::format(R"("{}" a b)", tool.path)).c_str()) ==
            tool.path);
};

ZEST_CASE(command_line_program_is_looked_for_as_create_process_does) {
    auto program = ct::resolve_program(nullptr, LR"("cmd" /c echo)");

    ZASSERT(program.has_value());
    ZEXPECT(program->ends_with(R"(\cmd.exe)"));
};

ZEST_CASE(program_found_nowhere_is_left_to_create_process) {
    ZEXPECT(ct::resolve_program(nullptr, L"catter-no-such-program a b") == std::nullopt);
    ZEXPECT(ct::resolve_program(L"catter-no-such-program.exe", L"a b") == std::nullopt);
};

ZEST_CASE(arguments_are_those_of_the_command_line) {
    ZEXPECT(ct::arguments_of(L"tool.exe", LR"(tool "a b" c\"d)") ==
            std::vector<std::string>{"tool", "a b", R"(c"d)"});
};

ZEST_CASE(arguments_without_a_command_line_are_those_of_the_application_name) {
    ZEXPECT(ct::arguments_of(LR"(C:\tools\tool.exe)", nullptr) ==
            std::vector<std::string>{R"(C:\tools\tool.exe)"});
};

ZEST_CASE(unicode_environment_block_is_read_as_utf16) {
    const wchar_t block[] = L"=C:=C:\\work\0PATH=C:\\bin\0NAME=\u00e9t\u00e9\0";

    ZEXPECT(ct::environment_of(block, true) ==
            std::vector<std::string>{R"(=C:=C:\work)", R"(PATH=C:\bin)", "NAME=\u00e9t\u00e9"});
};

ZEST_CASE(ansi_environment_block_is_read_in_the_ansi_code_page) {
    const char block[] = "PATH=C:\\bin\0NAME=value\0";

    ZEXPECT(ct::environment_of(block, false) ==
            std::vector<std::string>{R"(PATH=C:\bin)", "NAME=value"});
};

ZEST_CASE(no_environment_block_is_this_process_environment) {
    kotatsu::sys::set_env("CATTER_ENVIRONMENT_OF", "\u00e9t\u00e9");

    ZEXPECT(std::ranges::contains(ct::environment_of(nullptr, true),
                                  "CATTER_ENVIRONMENT_OF=\u00e9t\u00e9"));
};

ZEST_CASE(environment_block_reads_back_as_written) {
    const std::vector<std::string> variables{R"(=C:=C:\work)", "NAME=\u00e9t\u00e9"};
    const auto block = ct::environment_block(variables);

    std::wstring expected = L"=C:=C:\\work";
    expected += L'\0';
    expected += L"NAME=\u00e9t\u00e9";
    expected += L'\0';
    expected += L'\0';
    ZEXPECT(block == expected);
    ZEXPECT(ct::environment_of(block.data(), true) == variables);
};

ZEST_CASE(environment_block_is_sorted_by_name_ignoring_case) {
    const std::vector<std::string> variables{"b=2", "A=1", "=C:=C:\\work", "a_b=3"};

    ZEXPECT(ct::environment_of(ct::environment_block(variables).data(), true) ==
            std::vector<std::string>{R"(=C:=C:\work)", "A=1", "a_b=3", "b=2"});
};

ZEST_CASE(empty_environment_block_is_two_nuls) {
    ZEXPECT(ct::environment_block({}) == std::wstring(2, L'\0'));
};

ZEST_CASE(directory_is_made_absolute_from_the_current_one) {
    std::string current;
    kotatsu::fs::current_dir(current);
    std::string expected = current;
    kotatsu::fs::path::push(expected, "sub");

    ZEXPECT(ct::directory_of(L"sub") == expected);
    ZEXPECT(ct::directory_of(nullptr) == current);
};
};  // ZEST_SUITE(create_process)

}  // namespace
